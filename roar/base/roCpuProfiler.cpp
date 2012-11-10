#include "pch.h"
#include "roCpuProfiler.h"
#include "roArray.h"
#include "roStringFormat.h"
#include "roTaskPool.h"
#include "roTypeCast.h"
#include "../platform/roPlatformHeaders.h"

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
	float peakInclusiveTime() const;	// NOTE: Under then current system, it's hard to do peak self time

	float _inclusiveTime;
	float _peakInclusiveTime;
	mutable StopWatch _stopWatch;
	mutable RecursiveMutex mutex;
};	// CallstackNode

CallstackNode::CallstackNode(const char name[], CallstackNode* parent)
	: name(name)
	, parent(parent), firstChild(NULL), sibling(NULL)
	, recursionCount(0), callCount(0)
	, _inclusiveTime(0), _peakInclusiveTime(0)
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
		_stopWatch.reset();

	++callCount;
}

void CallstackNode::end()
{
	if(recursionCount == 0) {
		float elasped = _stopWatch.getFloat();
		_inclusiveTime += elasped;

		_peakInclusiveTime = roMaxOf2(_peakInclusiveTime, elasped);
	}
}

void CallstackNode::reset()
{
	CallstackNode* n1, *n2;
	{	// Race with CpuProfiler::begin(), CpuProfiler::end()
		ScopeRecursiveLock lock(mutex);
		callCount = 0;
		_inclusiveTime = 0;
		_peakInclusiveTime = 0;
		_stopWatch.reset();
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

	// If the recursion count is not zero, means the profile scope
	// hasn't finish yet, at the moment we try to generate the report.
	// This happens in multi-thread situation.
	return _inclusiveTime + _stopWatch.getFloat();
}

float CallstackNode::peakInclusiveTime() const
{
	if(recursionCount == 0)
		return _peakInclusiveTime;

	float elasped = _stopWatch.getFloat();
	return roMaxOf2(_peakInclusiveTime, elasped);
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
	TlsStruct(const char* name="THREAD")
		: recurseCount(0)
		, _currentNode(NULL), _threadName(name)
	{}

	CallstackNode* currentNode()
	{
		if(_currentNode)
			return _currentNode;

		// If node is null, means a new thread is started
		_currentNode = reinterpret_cast<CallstackNode*>(_profiler->_rootNode);
		_profiler->_begin(_threadName.c_str());

		return _currentNode;
	}

	CallstackNode* setCurrentNode(CallstackNode* node) {
		return _currentNode = node;
	}

	roSize recurseCount;
	String _threadName;	// We use a string variable here so every thread name are in unique memory
	CallstackNode* _currentNode;
};	// TlsStruct

DWORD _tlsIndex = 0;
RecursiveMutex _mutex;
TinyArray<TlsStruct, 64> _tlsStructs;

}	// namespace

CpuProfilerScope::CpuProfilerScope(const char name[])
{
	if(_profiler && _profiler->enable) {
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
	: enable(true)
	, _rootNode(NULL)
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

		if(!_tlsStructs.pushBack())
			return NULL;

		tls = &_tlsStructs.back();
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
	_rootNode = new CallstackNode("ROOT");

	reset();

	// NOTE: We assume CpuProfiler::init() will be invoked in the main thread
	_tlsStruct()->_threadName = "MAIN THREAD";	// Customize the name for main thread

	// Make sure the main thread appear first on the report
	reinterpret_cast<CallstackNode*>(_rootNode)->getChildByName(_tlsStruct()->_threadName.c_str());

	return Status::ok;
}

void CpuProfiler::tick()
{
	if(enable)
		++_frameCount;
}

void CpuProfiler::reset()
{
	_frameCount = 0;
	_stopWatch.reset();

	if(_rootNode)
		reinterpret_cast<CallstackNode*>(_rootNode)->reset();
}

float CpuProfiler::fps() const
{
	return _frameCount / _stopWatch.getFloat();
}

float CpuProfiler::timeSinceLastReset() const
{
	return (float)_stopWatch.getFloat();
}

String CpuProfiler::report(roSize nameLength, float skipMargin) const
{
	CpuProfilerScope cpuProfilerScope(__FUNCTION__);

	float fps_ = fps();

	String str;
	strFormat(str, "FPS: {}\n", static_cast<size_t>(fps_));

	String name = "Name";
	strPaddingRight(name, ' ', nameLength);

	const char* format = "{}{pr12 }{pr12 }{pr12 }{pr12 }{pr12 }{pr12 }\n";
	strFormat(str, format,
		name.c_str(), "TT/F %", "ST/F %", "TT/C", "ST/C", "Peak TT/C", "C/F"
	);

	CallstackNode* n = reinterpret_cast<CallstackNode*>(_rootNode);
	float percent = 100 * fps_;

	// Keep track the stack to print a nice tree structure
	TinyArray<CallstackNode*, 16> indentNodes;

	if(_frameCount) do
	{
		// Race with ThreadedCpuProfiler::begin() and ThreadedCpuProfiler::end()
		ScopeRecursiveLock lock(n->mutex);

		roSize callDepth = n->callDepth();
		indentNodes.resize(roMaxOf2(callDepth + 1, indentNodes.size()), NULL);
		indentNodes[callDepth] = n;

		float selfTime = n->selfTime();
		float inclusiveTime = n->inclusiveTime();

		if(n == _rootNode) {
			inclusiveTime = -selfTime;
			selfTime = 0;
		}

		// Skip node that have total time less than 1%
		if(inclusiveTime / _frameCount * percent >= skipMargin)
		{
			// Print out the tree structure
			name.clear();
			for(roSize i=0; i<callDepth; ++i)
			{
				if(indentNodes.isInRange(i+1) && indentNodes[i+1]->sibling) {
					if(i == callDepth - 1)
						name.append('\xC3');	// Char 195: |-
					else
						name.append('\xB3');	// Char 179: |
				}
				else if(indentNodes[i] == n->parent)
					name.append('\xC0');		// Char 192: |_
				else
					name.append(' ');
			}
			name.append(n->name);

			strPaddingRight(name, '\xFA', nameLength);	// Char 250: *
			strFormat(str, format,
				name,
				inclusiveTime / _frameCount * percent,
				selfTime / _frameCount * percent,
				n->callCount == 0 ? 0 : inclusiveTime / n->callCount,
				n->callCount == 0 ? 0 : selfTime / n->callCount,
				n->peakInclusiveTime(),
				float(n->callCount) / _frameCount
			);
		}

		n = CallstackNode::traverse(n);
	} while(n);

	return str;
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

	// NOTE: Is is possible that _end() is called without a prior _begin() because of the 'enable' option
	if(node->recursionCount == 0)
		return;

	node->recursionCount--;

	// Only back to the parent when the current node is not inside a recursive function
	if(node->recursionCount == 0) {
		node->end();
		tls->setCurrentNode(node->parent);
	}
}

}	// namespace ro
