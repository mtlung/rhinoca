#include "pch.h"
#include "roString.h"
#include "roStringUtility.h"
#include "roArray.h"
#include "roAtomic.h"
#include "roMemory.h"
#include "roMutex.h"
#include "roReflection.h"
#include "roStringUtility.h"
#include "roTypeCast.h"
#include "roUtility.h"
#include "../math/roMath.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

namespace ro {

static DefaultAllocator _allocator;
static const char _emptyString[16] = { 0 };	// Extended size to let debug visualizer show correctly
static roSize _zero = 0;

String::String()
	: _cstr(const_cast<char*>(_emptyString))
{
}

String::String(const char* str)
	: _cstr(const_cast<char*>(_emptyString))
{
	roVerify(assign(str, roStrLen(str)));
}

String::String(const char* str, roSize count)
	: _cstr(const_cast<char*>(_emptyString))
{
	roVerify(assign(str, count));
}

String::String(const String& str)
	: _cstr(const_cast<char*>(_emptyString))
{
	roVerify(assign(str._str(), str.size()));
}

String::~String()
{
	clear();
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

char* String::_str() const
{
	if(!_cstr)
		return NULL;
	if(_cstr == _emptyString)
		return (char*)_emptyString;
	return _cstr + sizeof(roSize) * 2;
}

roSize& String::_size() const
{
	return _cstr ? *((roSize*)_cstr) : (roSize&)_zero;
}

roSize& String::_capacity() const
{
	return _cstr ? *(((roSize*)_cstr) + 1) : (roSize&)_zero;
}

Status String::_reserve(roSize size, bool forceRealloc)
{
	roSize currentCap = _capacity();
	roSize desiredBytes = sizeof(roSize) * 2 + roSize(size * 1.5) + 1;

	if(currentCap < desiredBytes || forceRealloc) {
		roSize currentSize = _size();
		char* newPtr = _allocator.realloc(currentCap ? _cstr : NULL, currentCap, desiredBytes);
		if(!newPtr) return Status::not_enough_memory;
		_cstr = newPtr;
		_capacity() = desiredBytes;
		_size() = currentSize;
	}

	return Status::ok;
}

Status String::reserve(roSize size)
{
	return _reserve(size, false);
}

Status String::resize(roSize size)
{
	if(size == 0) {
		clear();
		return Status::ok;
	}

	Status st = reserve(size); if(!st) return st;
	_size() = size;
	_str()[size] = '\0';

	return Status::ok;
}

Status String::incSize(roSize inc)
{
	return resize(size() + inc);
}

Status String::decSize(roSize dec)
{
	dec = roClampMax(dec, size());
	return resize(size() - dec);
}

void String::condense()
{
	roSize minSize = roMinOf2(roStrLen(_str()), size());
	_reserve(minSize, true);
	resize(minSize);
}

Status String::append(char c)
{
	return append(c, 1);
}

Status String::append(char c, roSize repeat)
{
	roSize currentSize = _size();
	Status st = resize(currentSize + repeat); if(!st) return st;

	char* p = _str() + currentSize;
	for(roSize i=0; i<repeat; ++i)
		p[i] = c;

	return Status::ok;
}

Status String::append(const char* str)
{
	return insert(size(), str);
}

Status String::append(const char* str, roSize count)
{
	return insert(size(), str, count);
}

Status String::assign(const char* str)
{
	return assign(str, roStrLen(str));
}

Status String::assign(const char* str, roSize count)
{
	roAssert(count <= roStrLen(str));

	if(count) {
		Status st = resize(count); if(!st) return st;
		char* p = _str();
		roVerify(roStrnCpy(p, str, count) == p);
	}
	else
		clear();

	return Status::ok;
}

Status String::insert(roSize idx, char c)
{
	return insert(idx, &c, 1);
}

Status String::insert(roSize idx, const char* str)
{
	return insert(idx, str, roStrLen(str));
}

Status String::insert(roSize idx, const char* str, roSize count)
{
	roSize size_ = size();
	if(idx > size_)
		return Status::invalid_parameter;

	roSize movCount = size_ - idx;
	resize(size_ + count);
	char* p = c_str();
	roMemmov(p + idx + count, p + idx, movCount);
	roMemcpy(p + idx, str, count);

	return Status::ok;
}

void String::clear()
{
	if(_capacity())
		_allocator.free(_cstr);
	_cstr = const_cast<char*>(_emptyString);
}

void String::erase(roSize offset, roSize count)
{
	if(count == 0)
		return;

	roSize currentSize = _size();
	roAssert(offset < currentSize);
	if(count > currentSize - offset)
		count = currentSize - offset;

	if(const roSize newLen = currentSize - count) {
		char* p = _str();
		char* a = p + offset;
		char* b = a + count;
		memmove(a, b, num_cast<roSize>(p + currentSize - b));
		resize(newLen);
	}
	else
		clear();
}

void String::popBack(roSize count)
{
	erase(size() - 1, count);
}

roSize String::find(char c, roSize offset) const
{
	const char* p = _str();
	const char* ret = roStrChr(p + offset, c);
	return ret ? ret - p : npos;
}

roSize String::find(const char* str, roSize offset) const
{
	const char* p = _str();
	const char* ret = roStrStr(p + offset, str);
	return ret ? ret - p : npos;
}

roSize String::rfind(char c, roSize offset) const
{
	char str[] = { c, '\0' };
	return rfind(str, offset);
}

roSize String::rfind(const char* str, roSize offset) const
{
	roSize l1 = roMinOf2(offset, _size());
	roSize l2 = roStrLen(str);

	if(l2 > l1) return npos;

	const char* p = _str();
	for(char* s = _str() + l1 - l2; s >= p; --s)
		if(roStrnCmp(s, str, l2) == 0)
			return num_cast<roSize>(s - p);
	return npos;
}

String String::substr(roSize offset, roSize count) const
{
	const char* p = _str();
	if(count == npos)
		return String(p + offset);
	return String(p + offset, count);
}

String& String::operator+=(const char* str)
{
	if(const roSize len = roStrLen(str)) {
		const roSize oldLen = _size();
		resize(oldLen + len);
		char* p = _str();
		for(roSize i=0; i<len; ++i)
			p[oldLen + i] = str[i];
	}
	return *this;
}

String& String::operator+=(const String& str)
{
	return *this += str._str();
}

Status String::fromUtf16(const roUint16* src, roSize maxSrcLen)
{
	roSize len = 0;
	Status st = roUtf16ToUtf8(NULL, len, src, maxSrcLen); if(!st) return st;

	resize(len);
	st = roUtf16ToUtf8(_str(), len, src, maxSrcLen); if(!st) return st;

	return Status::ok;
}

Status String::toUtf16(roUint16* dst, roSize& dstLen)
{
	return roUtf8ToUtf16(dst, dstLen, _str(), roSize(-1));
}

bool String::operator==(const char* rhs) const
{
	return roStrCmp(_str(), rhs) == 0;
}

bool String::operator==(const String& rhs) const
{
	return roStrCmp(_str(), rhs._str()) == 0;
}


RangedString::operator String()
{
	if(begin && end && (end > begin))
		return String(begin, end - begin);
	return String();
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

	ConstStringHashTable() : _count(0), _buckets(1, NULL), _nullNode(add(""))
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

	Node& add(const char* str, roSize count=0)
	{
		if(!str)
			return _nullNode;

		const StringHash hash = stringHash(str, count);
		const roSize length = count == 0 ? roStrLen(str) : count;

		ScopeLock lock(_mutex);
		const roSize index = hash % _buckets.size();

		// Find any string with the same hash value
		for(Node* n = _buckets[index]; n; n = n->next) {
			if(n->hash == hash) {
				roAssert(roStrnCmp(n->stringValue(), str, length) == 0 && "String hash collision in ConstString" );
				return *n;
			}
		}

		if(Node* n = _allocator.malloc(sizeof(Node) + length + 1).cast<Node>()) {
			memcpy((void*)n->stringValue(), str, length);
			((char*)n->stringValue())[length] = '\0';
			n->hash = hash;
			n->lowerCaseHash = 0;	// We will assign it lazily
			n->refCount = 0;
			n->size = length;

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
		Array<Node*> newBuckets;
		newBuckets.resize(bucketSize, NULL);

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

ConstString::ConstString(const char* str, roSize count)
	: _node(&_constStringHashTable().add(str, count))
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

ConstString::operator const char*() const {
	return c_str();
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

roSize ConstString::size() const {
	return _node->size;
}

bool ConstString::isEmpty() const {
	return *_node->stringValue() == '\0';
}

bool ConstString::operator==(const StringHash& rhs) const	{ return hash() == rhs; }
bool ConstString::operator==(const ConstString& rhs) const	{ return hash() == rhs.hash(); }
bool ConstString::operator> (const ConstString& rhs) const	{ return hash() > rhs.hash(); }
bool ConstString::operator< (const ConstString& rhs) const	{ return hash() < rhs.hash(); }

roStatus Reflection::serialize_roString(Serializer& se, Field& field, void* fieldParent)
{
	Field f = field;	// TODO: Optimize this copying
	const String* string = f.getConstPtr<String>(fieldParent);
	if(!string) return roStatus::pointer_is_null;
	f.offset = 0;

	const char* str = string->c_str();
	roStatus st = Reflection::serialize_string(se, f, (void*)(&str));
	if(!st) return st;

	if(se.isReading)
		st = const_cast<String*>(string)->assign(str);

	return st;
}

}	// namespace ro
