#include "pch.h"
#include "rhstring.h"
#include "atomic.h"
#include "mutex.h"
#include "vector.h"
#include <string.h>

static char* _emptyString = "";

String::String()
	: _cstr(_emptyString)
	, _length(0)
{
}

String::String(const char* str)
{
	_length = strlen(str);

	if(_length) {
		_cstr = (char*)rhinoca_malloc(_length + 1);
		VERIFY(strncpy(_cstr, str, _length) == _cstr);
		_cstr[_length] = '\0';
	}
	else
		_cstr = _emptyString;
}

String::String(const char* str, unsigned count)
{
	ASSERT(count <= strlen(str));
	_length = count;

	if(_length) {
		_cstr = (char*)rhinoca_malloc(_length + 1);
		VERIFY(strncpy(_cstr, str, _length) == _cstr);
		_cstr[_length] = '\0';
	}
	else
		_cstr = _emptyString;
}

String::String(const String& str)
{
	_length = str._length;

	if(_length) {
		_cstr = (char*)rhinoca_malloc(_length + 1);
		VERIFY(strncpy(_cstr, str._cstr, _length) == _cstr);
		_cstr[_length] = '\0';
	}
	else
		_cstr = _emptyString;
}

String::~String()
{
	if(_length > 0)
		rhinoca_free(_cstr);
}

String& String::operator=(const char* str)
{
	String tmp(str);
	this->swap(tmp);
	return *this;
}

String& String::operator=(const String& str)
{
	String tmp(str);
	this->swap(tmp);
	return *this;
}

void String::resize(unsigned size)
{
	if(size == 0)
		clear();
	else {
		_cstr = (char*)rhinoca_realloc(_length ? _cstr : NULL, _length + 1, size + 1);
		_length = size;
		_cstr[_length] = '\0';
	}
}

String& String::append(const char* str, size_type count)
{
	if(count > 0) {
		ASSERT(count <= strlen(str));
		_cstr = (char*)rhinoca_realloc(_length ? _cstr : NULL, _length + 1, _length + count + 1);
		memcpy(_cstr + _length, str, count);
		_length += count;
		_cstr[_length] = '\0';
	}

	return *this;
}

void String::clear()
{
	erase(0);
}

String& String::erase(size_type offset, size_type count)
{
	if(_length == 0)
		return *this;

	ASSERT(offset < _length);
	if(count > _length - offset)
		count = _length - offset;

	if(const unsigned newLen = _length - count) {
		char* a = _cstr + offset;
		char* b = a + count;
		memmove(a, b, _cstr + _length - b);
		_cstr = (char*)rhinoca_realloc(_length ? _cstr : NULL, _length + 1, newLen + 1);
		_length = newLen;
		_cstr[_length] = '\0';
	}
	else
		clear();

	return *this;
}

String::size_type String::find(char c, size_type offset) const
{
	char str[] = { c, '\0' };
	return find(str, offset);
}

String::size_type String::find(const char* str, size_type offset) const
{
	const char* ret = strstr(_cstr + offset, str);
	return ret ? ret - _cstr : npos;
}

String::size_type String::rfind(char c, size_type offset) const
{
	char str[] = { c, '\0' };
	return rfind(str, offset);
}

String::size_type String::rfind(const char* str, size_type offset) const
{
	unsigned l1 = offset > _length ? _length : offset;
	unsigned l2 = strlen(str);

	if(l2 > l1) return npos;

	for(char* s = _cstr + l1 - l2; s >= _cstr; --s)
		if(strncmp(s, str, l2) == 0)
			return s - _cstr;
	return npos;
}

String String::substr(size_type offset, size_type count) const
{
	if(count == npos)
		return String(_cstr + offset);
	return String(_cstr + offset, count);
}

String& String::operator+=(const char* str)
{
	if(const unsigned len = strlen(str)) {
		_cstr = (char*)rhinoca_realloc(_length ? _cstr : NULL, _length + 1, _length + len + 1);
		VERIFY(strcat(_cstr, str) == _cstr);
		_length += len;
	}
	return *this;
}

String& String::operator+=(const String& str)
{
	if(!str.empty()) {
		_cstr = (char*)rhinoca_realloc(_length ? _cstr : NULL, _length + 1, _length + str._length + 1);
		VERIFY(strcat(_cstr, str._cstr) == _cstr);
		_length += str._length;
	}
	return *this;
}

void String::swap(String& rhs)
{
	char* s = _cstr;
	unsigned l = _length;

	_cstr = rhs._cstr;
	_length = rhs._length;

	rhs._cstr = s;
	rhs._length = l;
}

bool strToBool(const char* str, bool defaultValue)
{
	if(!str)
		return defaultValue;
	else if(strcasecmp(str, "true") == 0)
		return true;
	else if(strcasecmp(str, "false") == 0)
		return false;

	return atof(str, defaultValue) > 0 ? true : false;
}

char* rstrstr(char* __restrict str1, const char* __restrict str2)
{
	size_t s1len = strlen(str1);
	size_t s2len = strlen(str2);
	char* s;

	if(s2len > s1len)
		return NULL;
	for(s = str1 + s1len - s2len; s >= str1; --s)
		if(strncmp(s, str2, s2len) == 0)
			return s;
	return NULL;
}

char* replaceCharacterWithStr(const char* str, const char charArray[], const char** replacements)
{
	unsigned orgLen = strlen(str);
	unsigned charCount = strlen(charArray);

	Vector<char> buf;
	buf.reserve(orgLen);

	while(*str != '\0')
	{
		bool converted = false;
		for(unsigned i=0; i<charCount; ++i) {
			if(*str == charArray[i]) {
				for(const char* s = replacements[i]; *s != '\0'; ++s)
					buf.push_back(*s);
				converted = true;
				break;
			}
		}
		if(!converted)
			buf.push_back(*str);

		++str;
	}

	char* ret = (char*)rhinoca_malloc(buf.size() + 1);
	strncpy(ret, &buf[0], buf.size())[buf.size()] = '\0';
	return ret;
}

StringHash::StringHash(const char* buf, size_t len)
{
	rhuint32 hash_ = 0;
	len = len == 0 ? size_t(-1) : len;

	for(size_t i=0; i<len && buf[i] != '\0'; ++i) {
		//hash = hash * 65599 + buf[i];
		hash_ = buf[i] + (hash_ << 6) + (hash_ << 16) - hash_;
	}
	hash = hash_;
}

StringHash::StringHash(const wchar_t* buf, size_t len)
{
	rhuint32 hash_ = 0;
	len = len == 0 ? size_t(-1) : len;

	for(size_t i=0; i<len && buf[i] != L'\0'; ++i) {
		//hash = hash * 65599 + buf[i];
		hash_ = buf[i] + (hash_ << 6) + (hash_ << 16) - hash_;
	}
	hash = hash_;
}

struct FixString::Node
{
	rhuint32 hashValue;
	Node* next;
	AtomicInteger refCount;
	size_t size;	//!< Length of the string
	const char* stringValue() const {
		return reinterpret_cast<const char*>(this + 1);
	}
};	// Node

namespace {

class FixStringHashTable
{
public:
	typedef FixString::Node Node;

	FixStringHashTable() : mCount(0), mBuckets(1), mNullNode(add(""))
	{
		++mNullNode.refCount;
	}

	~FixStringHashTable()
	{
		remove(mNullNode);
		ASSERT(mCount == 0 && "All instance of FixString should be destroyed before FixStringHashTable");
	}

	Node& find(rhuint32 hashValue) const
	{
		ScopeLock lock(mMutex);

		const size_t index = hashValue % mBuckets.size();
		for(Node* n = mBuckets[index]; n; n = n->next) {
			if(n->hashValue != hashValue)
				continue;
			return *n;
		}
		return mNullNode;
	}

	Node& add(const char* str)
	{
		if(!str)
			return mNullNode;

		const rhuint32 hashValue = StringHash(str, 0).hash;

		ScopeLock lock(mMutex);
		const size_t index = hashValue % mBuckets.size();

		// Find any string with the same hash value
		for(Node* n = mBuckets[index]; n; n = n->next) {
			if(n->hashValue == hashValue) {
				ASSERT(strcmp(n->stringValue(), str) == 0 && "String hash collision in FixString" );
				return *n;
			}
		}

		const size_t length = strlen(str) + 1;
		if(Node* n = (Node*)rhinoca_malloc(sizeof(Node) + length)) {
			memcpy((void*)n->stringValue(), str, length);
			n->hashValue = hashValue;
			n->refCount = 0;
			n->size = length - 1;

			n->next = mBuckets[index];
			mBuckets[index] = n;
			++mCount;

			// Enlarge the bucket if necessary
			if(mCount * 2 > mBuckets.size() * 3)
				resizeBucket(mBuckets.size() * 2);

			return *n;
		}
		return mNullNode;
	}

	void remove(Node& node)
	{
		if(--node.refCount != 0)
			return;

		ScopeLock lock(mMutex);
		const size_t index = node.hashValue % mBuckets.size();
		Node* last = NULL;

		for(Node* n = mBuckets[index]; n; last = n, n = n->next) {
			if(n->hashValue == node.hashValue) {
				if(last)
					last->next = n->next;
				else
					mBuckets[index] = n->next;
				rhinoca_free(n);
				--mCount;
				return;
			}
		}
		ASSERT(false);
	}

	void resizeBucket(size_t bucketSize)
	{
		Vector<Node*> newBuckets(bucketSize, NULL);

		ASSERT(mMutex.isLocked());
		for(size_t i=0; i<mBuckets.size(); ++i) {
			for(Node* n = mBuckets[i]; n; ) {
				Node* next = n->next;
				const size_t index = n->hashValue % bucketSize;
				n->next = newBuckets[index];
				newBuckets[index] = n;
				n = next;
			}
		}

		mBuckets.swap(newBuckets);
	}

	mutable Mutex mMutex;
	size_t mCount;	//!< The actual number of elements in this table, can be <=> mBuckets.size()
	Vector<Node*> mBuckets;
	Node& mNullNode;
};	// FixStringHashTable

static FixStringHashTable& gFixStringHashTable() {
	static FixStringHashTable singleton;
	return singleton;
}

}	// namespace

FixString::FixString()
	: mNode(&gFixStringHashTable().add(""))
{
	++mNode->refCount;
#ifndef NDEBUG
	debugStr = c_str();
#endif
}

FixString::FixString(const char* str)
	: mNode(&gFixStringHashTable().add(str))
{
	++mNode->refCount;
#ifndef NDEBUG
	debugStr = c_str();
#endif
}

FixString::FixString(rhuint32 hashValue)
	: mNode(&gFixStringHashTable().find(hashValue))
{
	++mNode->refCount;
#ifndef NDEBUG
	debugStr = c_str();
#endif
}

FixString::FixString(const FixString& rhs)
	: mNode(rhs.mNode)
{
	++mNode->refCount;
#ifndef NDEBUG
	debugStr = c_str();
#endif
}

FixString::~FixString() {
	gFixStringHashTable().remove(*mNode);
}

FixString& FixString::operator=(const char* rhs)
{
	FixString tmp(rhs);
	return *this = tmp;
}

FixString& FixString::operator=(const FixString& rhs)
{
	gFixStringHashTable().remove(*mNode);
	mNode = rhs.mNode;
	++mNode->refCount;
#ifndef NDEBUG
	debugStr = c_str();
#endif
	return *this;
}

FixString& FixString::operator=(const StringHash& stringHash)
{
	FixString tmp(stringHash);
	return *this = tmp;
}

const char* FixString::c_str() const {
	return mNode->stringValue();
}

rhuint32 FixString::hashValue() const {
	return mNode->hashValue;
}

size_t FixString::size() const {
	return mNode->size;
}

bool FixString::empty() const {
	return *mNode->stringValue() == '\0';
}

FixString FixString::upperCase() const
{
	// TODO: Can make use of stack allocator
	char* newStr = (char*)rhinoca_malloc(size() + 1);
	strcpy(newStr, c_str());	// strcpy should copy \0
	toupper(newStr);
	FixString ret(newStr);
	rhinoca_free(newStr);
	return ret;
}

FixString FixString::lowerCase() const
{
	// TODO: Can make use of stack allocator
	char* newStr = (char*)rhinoca_malloc(size() + 1);
	strcpy(newStr, c_str());	// strcpy should copy \0
	tolower(newStr);
	FixString ret(newStr);
	rhinoca_free(newStr);
	return ret;
}

bool FixString::operator==(const StringHash& h) const	{	return hashValue() == h;	}
bool FixString::operator==(const FixString& rhs) const	{	return hashValue() == rhs.hashValue();	}
bool FixString::operator> (const FixString& rhs) const	{	return hashValue() > rhs.hashValue();	}
bool FixString::operator< (const FixString& rhs) const	{	return hashValue() < rhs.hashValue();	}
