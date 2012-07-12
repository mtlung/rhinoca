#include "pch.h"
#include "roCpuProfiler.h"
#include "roArray.h"
#include "roTaskPool.h"
#include "roTypeCast.h"
#include "../platform/roPlatformHeaders.h"
#include <sstream>
#include <iomanip>

namespace ro {

namespace {

struct CallstackNode
{
	CallstackNode(const char name[], CallstackNode* parent=NULL);
	~CallstackNode();

// Operations
	CallstackNode* getChildByName(const char name[]);

	void begin();
	void end();

	void reset();

	static CallstackNode* traverse(CallstackNode* n);

// Attributes
	const char* const name;

	CallstackNode* parent;
	CallstackNode* firstChild;
	CallstackNode* sibling;

	roSize recursionCount;
	roSize callCount;
	roSize callDepth() const;
	float selfTime() const;
	float inclusiveTime() const;

	float _inclusiveTime;
	StopWatch stopWatch;
	mutable RecursiveMutex mutex;
};	// CallstackNode

CallstackNode::CallstackNode(const char name[], CallstackNode* parent)
	: name(name)
	, parent(parent), firstChild(NULL), sibling(NULL)
	, recursionCount(0), callCount(0), _inclusiveTime(0)
{}

CallstackNode::~CallstackNode()
{
	delete firstChild;
	delete sibling;
}

CallstackNode* CallstackNode::getChildByName(const char name_[])
{
	{	// Try to find a node with the supplied name
		CallstackNode* n = firstChild;
		while(n) {
			// NOTE: We are comparing the string's pointer directly
			if(n->name == name_)
				return n;
			n = n->sibling;
		}
	}

	{	// Search for ancestor with non zero recursion
		CallstackNode* n = parent;
		while(n) {
			if(n->name == name_ && n->recursionCount > 0)
				return n;
			n = n->parent;
		}
	}

	{	// We didn't find it, so create a new one
		CallstackNode* node = new CallstackNode(name_, this);

		if(firstChild) {
			CallstackNode* lastChild;
			CallstackNode* tmp = firstChild;
			do {
				lastChild = tmp;
				tmp = tmp->sibling;
			} while(tmp);
			lastChild->sibling = node;
		} else {
			firstChild = node;
		}

		return node;
	}
}

void CallstackNode::begin()
{
	// Start the timer for the first call, ignore all later recursive call
	if(recursionCount == 0)
		stopWatch.reset();

	++callCount;
}

void CallstackNode::end()
{
	if(recursionCount == 0)
		_inclusiveTime += stopWatch.getFloat();
}

void CallstackNode::reset()
{
	CallstackNode* n1, *n2;
	{	// Race with CpuProfiler::begin(), CpuProfiler::end()
		ScopeRecursiveLock lock(mutex);
		callCount = 0;
		_inclusiveTime = 0;
		stopWatch.reset();
		n1 = firstChild;
		n2 = sibling;
	}

	if(n1) n1->reset();
	if(n2) n2->reset();
}

roSize CallstackNode::callDepth() const
{
	CallstackNode* p = parent;
	roSize depth = 0;
	while(p) {
		p = p->parent;
		++depth;
	}
	return depth;
}

float CallstackNode::selfTime() const
{
	// Loop and sum for all direct children
	float sum = 0;
	const CallstackNode* n = static_cast<CallstackNode*>(firstChild);
	while(n) {
		ScopeRecursiveLock lock(n->mutex);
		sum += n->inclusiveTime();
		n = n->sibling;
	}

	return inclusiveTime() - sum;
}

float CallstackNode::inclusiveTime() const
{
	if(recursionCount == 0)
		return _inclusiveTime;

	// If the recursion count is not zero, means the profil scope
	// hasn't finish yet, at the moment we try to generate the report.
	// This happens in multi-thread situation.
	return _inclusiveTime + stopWatch.getFloat();
}

CallstackNode* CallstackNode::traverse(CallstackNode* n)
{
	if(!n) return NULL;

	if(n->firstChild)
		n = n->firstChild;
	else {
		while(!n->sibling) {
			n = n->parent;
			if(!n)
				return NULL;
		}
		n = n->sibling;
	}
	return n;
}

static CpuProfiler* _profiler = NULL;

struct TlsStruct
{
	TlsStruct(const char* name="thread")
		: recurseCount(0)
		, _currentNode(NULL), _threadName(name)
	{}

	CallstackNode* currentNode()
	{
		// If node is null, means a new thread is started
		if(!_currentNode) {
			CallstackNode* rootNode = reinterpret_cast<CallstackNode*>(_profiler->_rootNode);
			roAssert(rootNode);
			recurseCount++;
			_currentNode = rootNode->getChildByName(_threadName.c_str());
			recurseCount--;
		}

		return _currentNode;
	}

	CallstackNode* setCurrentNode(CallstackNode* node) {
		return _currentNode = node;
	}

	roSize recurseCount;
	String _threadName;
	CallstackNode* _currentNode;
};	// TlsStruct

DWORD _tlsIndex = 0;
RecursiveMutex _mutex;
TinyArray<TlsStruct, 64> _tlsStructs;

}	// namespace

CpuProfilerScope::CpuProfilerScope(const char name[])
{
	if(_profiler) {
		_name = name;
		_profiler->_begin(name);
	}
	else
		_name = NULL;
}

CpuProfilerScope::~CpuProfilerScope()
{
	if(_profiler && _name)
		_profiler->_end();
}

CpuProfiler::CpuProfiler()
	: _rootNode(NULL)
	, _frameCount(0)
{
}

CpuProfiler::~CpuProfiler()
{
	shutdown();
}

TlsStruct* _tlsStruct()
{
	roAssert(_tlsIndex != 0);

	TlsStruct* tls = reinterpret_cast<TlsStruct*>(::TlsGetValue(_tlsIndex));

	if(!tls) {
		ScopeRecursiveLock lock(_mutex);

		// TODO: The current method didn't respect thread destruction
		roAssert(_tlsStructs.size() < _tlsStructs.capacity());
		if(_tlsStructs.size() >= _tlsStructs.capacity())
			return NULL;

		tls = &_tlsStructs.pushBack();
		::TlsSetValue(_tlsIndex, tls);
	}

	return tls;
}

Status CpuProfiler::init()
{
	shutdown();

	_profiler = this;
	_frameCount = 0;
	_tlsIndex = ::TlsAlloc();
	_rootNode = new CallstackNode("root");

	reset();

	return Status::ok;
}

void CpuProfiler::tick()
{
	++_frameCount;
}

void CpuProfiler::reset()
{
	_frameCount = 0;
	stopWatch.reset();

	if(_rootNode)
		reinterpret_cast<CallstackNode*>(_rootNode)->reset();
}

float CpuProfiler::fps() const
{
	return _frameCount / stopWatch.getFloat();
}

float CpuProfiler::timeSinceLastReset() const
{
	return (float)stopWatch.getFloat();
}

String CpuProfiler::report(roSize nameLength, float skipMargin) const
{
	using namespace std;
	ostringstream ss;

	float fps_ = fps();
	ss << "FPS: " << static_cast<size_t>(fps_) << endl;

	const std::streamsize percentWidth = 12;
	const std::streamsize floatWidth = 12;

	ss.flags(ios_base::left);
	ss	<< setw(std::streamsize(nameLength)) << "Name"
		<< setw(percentWidth)	<< "TT/F %"
		<< setw(percentWidth)	<< "ST/F %"
		<< setw(floatWidth)		<< "TT/C"
		<< setw(floatWidth)		<< "ST/C"
		<< setw(floatWidth)		<< "C/F"
		<< endl;

	CallstackNode* n = reinterpret_cast<CallstackNode*>(_rootNode);
	float percent = 100 * fps_;

	if(_frameCount) do
	{
		// Race with ThreadedCpuProfiler::begin() and ThreadedCpuProfiler::end()
		ScopeRecursiveLock lock(n->mutex);

		// Skip node that have total time less than 1%
//		if(n->_inclusiveTime / _frameCount * percent >= skipMargin)
		{
			float selfTime = n->selfTime();
			float inclusiveTime = n->inclusiveTime();

			std::streamsize callDepth = std::streamsize(n->callDepth());
			ss	<< setw(callDepth) << ""
				<< setw(std::streamsize(nameLength - callDepth)) << n->name
				<< setprecision(3)
				<< setw(percentWidth)	<< (inclusiveTime / _frameCount * percent)
				<< setw(percentWidth)	<< (selfTime / _frameCount * percent)
				<< setw(floatWidth)		<< (n->callCount == 0 ? 0 : inclusiveTime / n->callCount)
				<< setw(floatWidth)		<< (n->callCount == 0 ? 0 : selfTime / n->callCount)
				<< setprecision(2)
				<< setw(floatWidth-2)	<< (float(n->callCount) / _frameCount)
				<< endl;
		}

		n = CallstackNode::traverse(n);
	} while(n);

	return ss.str().c_str();
}

void CpuProfiler::shutdown()
{
	_profiler = NULL;

	if(_tlsIndex) ::TlsFree(_tlsIndex);
	_tlsIndex = 0;

	delete reinterpret_cast<CallstackNode*>(_rootNode);
	_rootNode = NULL;
}

void CpuProfiler::_begin(const char name[])
{
	TlsStruct* tls = _tlsStruct();
	CallstackNode* node = tls->currentNode();

	// Race with CpuProfiler::reset(), CpuProfiler::defaultReport()
	ScopeRecursiveLock lock(node->mutex);

	if(name != node->name) {
		CallstackNode* tmp = node->getChildByName(name);
		lock.swapMutex(tmp->mutex);
		node = tmp;

		// Only alter the current node, if the child node is not recursing
		if(node->recursionCount == 0)
			tls->setCurrentNode(node);
	}

	node->begin();
	node->recursionCount++;
}

void CpuProfiler::_end()
{
	TlsStruct* tls = _tlsStruct();
	CallstackNode* node = tls->currentNode();

	// Race with CpuProfiler::reset(), CpuProfiler::defaultReport()
	ScopeRecursiveLock lock(node->mutex);

	roAssert(node->recursionCount > 0);
	node->recursionCount--;

	// Only back to the parent when the current node is not inside a recursive function
	if(node->recursionCount == 0) {
		node->end();
		tls->setCurrentNode(node->parent);
	}
}

}	// namespace ro
