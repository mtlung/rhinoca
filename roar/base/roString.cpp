#include "pch.h"
#include "roString.h"
#include "roStringUtility.h"
#include "roArray.h"
#include "roAtomic.h"
#include "roMemory.h"
#include "roMutex.h"
#include "roStringUtility.h"
#include "roTypeCast.h"
#include "roUtility.h"
#include <string.h>

namespace ro {

static roDefaultAllocator _allocator;
static const char* _emptyString = "";

String::String()
	: _cstr(const_cast<char*>(_emptyString))
	, _length(0)
{
}

String::String(const char* str)
{
	_length = roStrLen(str);

	if(_length) {
		_cstr = _allocator.malloc(_length + 1);
		roVerify(roStrnCpy(_cstr, str, _length) == _cstr);
		_cstr[_length] = '\0';
	}
	else
		_cstr = const_cast<char*>(_emptyString);
}

String::String(const char* str, roSize count)
{
	roAssert(count <= roStrLen(str));
	_length = count;

	if(_length) {
		_cstr = _allocator.malloc(_length + 1);
		roVerify(roStrnCpy(_cstr, str, _length) == _cstr);
		_cstr[_length] = '\0';
	}
	else
		_cstr = const_cast<char*>(_emptyString);
}

String::String(const String& str)
{
	_length = str._length;

	if(_length) {
		_cstr = _allocator.malloc(_length + 1);
		roVerify(roStrnCpy(_cstr, str._cstr, _length) == _cstr);
		_cstr[_length] = '\0';
	}
	else
		_cstr = const_cast<char*>(_emptyString);
}

String::~String()
{
	if(_length > 0)
		_allocator.free(_cstr);
}

String& String::operator=(const char* str)
{
	String tmp(str);
	roSwap(*this, tmp);
	return *this;
}

String& String::operator=(const String& str)
{
	String tmp(str);
	roSwap(*this, tmp);
	return *this;
}

void String::resize(roSize size)
{
	if(size == 0)
		clear();
	else {
		_cstr = _allocator.realloc(_length ? _cstr : NULL, _length + 1, size + 1);
		_length = size;
		_cstr[_length] = '\0';
	}
}

String& String::append(const char* str, roSize count)
{
	if(count > 0) {
		if(str[count - 1] == '\0') --count;	// Don't copy the null terminator, we will explicitly put one
		_cstr = _allocator.realloc(_length ? _cstr : NULL, _length + 1, _length + count + 1);
		memcpy(_cstr + _length, str, count);
		_length += count;
		_cstr[_length] = '\0';
	}

	return *this;
}

String& String::assign(const char* str, roSize count)
{
	clear();
	return append(str, count);
}

void String::clear()
{
	erase(0);
}

String& String::erase(roSize offset, roSize count)
{
	if(_length == 0)
		return *this;

	roAssert(offset < _length);
	if(count > _length - offset)
		count = _length - offset;

	if(const roSize newLen = _length - count) {
		char* a = _cstr + offset;
		char* b = a + count;
		memmove(a, b, num_cast<roSize>(_cstr + _length - b));
		resize(newLen);
	}
	else
		clear();

	return *this;
}

roSize String::find(char c, roSize offset) const
{
	const char* ret = roStrChr(_cstr + offset, c);
	return ret ? ret - _cstr : npos;
}

roSize String::find(const char* str, roSize offset) const
{
	const char* ret = roStrStr(_cstr + offset, str);
	return ret ? ret - _cstr : npos;
}

roSize String::rfind(char c, roSize offset) const
{
	char str[] = { c, '\0' };
	return rfind(str, offset);
}

roSize String::rfind(const char* str, roSize offset) const
{
	roSize l1 = roMinOf2(offset, _length);
	roSize l2 = roStrLen(str);

	if(l2 > l1) return npos;

	for(char* s = _cstr + l1 - l2; s >= _cstr; --s)
		if(roStrnCmp(s, str, l2) == 0)
			return num_cast<roSize>(s - _cstr);
	return npos;
}

String String::substr(roSize offset, roSize count) const
{
	if(count == npos)
		return String(_cstr + offset);
	return String(_cstr + offset, count);
}

String& String::operator+=(const char* str)
{
	if(const roSize len = roStrLen(str)) {
		resize(_length + len);
		roVerify(strcat(_cstr, str) == _cstr);
	}
	return *this;
}

String& String::operator+=(const String& str)
{
	if(!str.isEmpty()) {
		resize(_length + str._length);
		roVerify(strcat(_cstr, str._cstr) == _cstr);
	}
	return *this;
}

bool String::fromUtf16(roUint16* src, roSize maxSrcLen)
{
	roSize len = 0;
	if(!roUtf16ToUtf8(NULL, len, src, maxSrcLen))
		return false;

	if(len == 0)
		return false;

	resize(len);
	if(!roUtf16ToUtf8(_cstr, len, src, maxSrcLen))
		return false;

	return true;
}


// ----------------------------------------------------------------------

struct ConstString::Node
{
	StringHash hash;
	StringHash lowerCaseHash;
	Node* next;
	AtomicInteger refCount;
	roSize size;	///< Length of the string
	const char* stringValue() const { return reinterpret_cast<const char*>(this + 1); }
};	// Node

struct ConstStringHashTable
{
	typedef ConstString::Node Node;

	ConstStringHashTable() : _count(0), _buckets(1,NULL), _nullNode(add(""))
	{
		++_nullNode.refCount;
	}

	~ConstStringHashTable()
	{
		remove(_nullNode);
		roAssert(_count == 0 && "All instance of ConstString should be destroyed before ConstStringHashTable");
	}

	Node& find(StringHash hash) const
	{
		ScopeLock lock(_mutex);

		const roSize index = hash % _buckets.size();
		for(Node* n = _buckets[index]; n; n = n->next) {
			if(n->hash != hash)
				continue;
			return *n;
		}
		return _nullNode;
	}

	Node& add(const char* str)
	{
		if(!str)
			return _nullNode;

		const StringHash hash = stringHash(str, 0);

		ScopeLock lock(_mutex);
		const roSize index = hash % _buckets.size();

		// Find any string with the same hash value
		for(Node* n = _buckets[index]; n; n = n->next) {
			if(n->hash == hash) {
				roAssert(strcmp(n->stringValue(), str) == 0 && "String hash collision in ConstString" );
				return *n;
			}
		}

		const roSize length = roStrLen(str) + 1;
		if(Node* n = _allocator.malloc(sizeof(Node) + length).cast<Node>()) {
			memcpy((void*)n->stringValue(), str, length);
			n->hash = hash;
			n->lowerCaseHash = 0;	// We will assign it lazily
			n->refCount = 0;
			n->size = length - 1;

			n->next = _buckets[index];
			_buckets[index] = n;
			++_count;

			// Enlarge the bucket if necessary
			if(_count * 2 > _buckets.size() * 3)
				resizeBucket(_buckets.size() * 2);

			return *n;
		}
		return _nullNode;
	}

	void remove(Node& node)
	{
		if(--node.refCount != 0)
			return;

		ScopeLock lock(_mutex);
		const roSize index = node.hash % _buckets.size();
		Node* last = NULL;

		for(Node* n = _buckets[index]; n; last = n, n = n->next) {
			if(n->hash == node.hash) {
				if(last)
					last->next = n->next;
				else
					_buckets[index] = n->next;
				_allocator.free(n);
				--_count;
				return;
			}
		}
		roAssert(false);
	}

	void resizeBucket(roSize bucketSize)
	{
		Array<Node*> newBuckets(bucketSize, NULL);

		roAssert(_mutex.isLocked());
		for(roSize i=0; i<_buckets.size(); ++i) {
			for(Node* n = _buckets[i]; n; ) {
				Node* next = n->next;
				const roSize index = n->hash % bucketSize;
				n->next = newBuckets[index];
				newBuckets[index] = n;
				n = next;
			}
		}

		roSwap(_buckets, newBuckets);
	}

	mutable Mutex _mutex;
	roSize _count;	///< The actual number of elements in this table, can be <=> _buckets.size()
	Array<Node*> _buckets;
	Node& _nullNode;
};	// ConstStringHashTable

static ConstStringHashTable& _constStringHashTable()
{
	static ConstStringHashTable singleton;
	return singleton;
}

ConstString::ConstString()
	: _node(&_constStringHashTable().add(""))
{
	++_node->refCount;
#if roDEBUG
	_debugStr = c_str();
#endif
}

ConstString::ConstString(const char* str)
	: _node(&_constStringHashTable().add(str))
{
	++_node->refCount;
#if roDEBUG
	_debugStr = c_str();
#endif
}

ConstString::ConstString(StringHash hash)
	: _node(&_constStringHashTable().find(hash))
{
	++_node->refCount;
#if roDEBUG
	_debugStr = c_str();
#endif
}

ConstString::ConstString(const ConstString& rhs)
	: _node(rhs._node)
{
	++_node->refCount;
#if roDEBUG
	_debugStr = c_str();
#endif
}

ConstString::~ConstString() {
	_constStringHashTable().remove(*_node);
}

ConstString& ConstString::operator=(const char* rhs)
{
	ConstString tmp(rhs);
	return *this = tmp;
}

ConstString& ConstString::operator=(const ConstString& rhs)
{
	_constStringHashTable().remove(*_node);
	_node = rhs._node;
	++_node->refCount;
#if roDEBUG
	_debugStr = c_str();
#endif
	return *this;
}

ConstString& ConstString::operator=(const StringHash& stringHash)
{
	ConstString tmp(stringHash);
	return *this = tmp;
}

const char* ConstString::c_str() const {
	return _node->stringValue();
}

StringHash ConstString::hash() const {
	return _node->hash;
}

StringHash ConstString::lowerCaseHash() const
{
	if(_node->lowerCaseHash == 0)
		_node->lowerCaseHash = stringLowerCaseHash(_node->stringValue(), _node->size);

	return _node->lowerCaseHash;
}

unsigned ConstString::size() const {
	return _node->size;
}

bool ConstString::isEmpty() const {
	return *_node->stringValue() == '\0';
}

bool ConstString::operator==(const StringHash& rhs) const	{ return hash() == rhs; }
bool ConstString::operator==(const ConstString& rhs) const	{ return hash() == rhs.hash(); }
bool ConstString::operator> (const ConstString& rhs) const	{ return hash() > rhs.hash(); }
bool ConstString::operator< (const ConstString& rhs) const	{ return hash() < rhs.hash(); }

}	// namespace ro
