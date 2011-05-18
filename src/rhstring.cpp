#include "pch.h"
#include "rhstring.h"
#include "atomic.h"
#include "mutex.h"
#include "vector.h"
#include <string.h>
#include <vector>

char* replaceCharacterWithStr(const char* str, const char charArray[], const char** replacements)
{
	unsigned orgLen = strlen(str);
	unsigned charCount = strlen(charArray);

	Vector<char> buf;
	buf.reserve(orgLen);

	while(*str != '\0')
	{
		for(unsigned i=0; i<charCount; ++i) {
			if(*str == charArray[i]) {
				for(const char* s = replacements[i]; *s != '\0'; ++s)
					buf.push_back(*s);
			}
			else
				buf.push_back(*str);
		}
		++str;
	}

	char* ret = (char*)rhinoca_malloc(buf.size() + 1);
	strcpy(ret, &buf[0]);
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
		std::vector<Node*> newBuckets(bucketSize, NULL);

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
	size_t mCount;	//!< The actuall number of elements in this table, can be <=> mBuckets.size()
	std::vector<Node*> mBuckets;
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

bool FixString::operator==(const StringHash& h) const	{	return hashValue() == h;	}
bool FixString::operator==(const FixString& rhs) const	{	return hashValue() == rhs.hashValue();	}
bool FixString::operator> (const FixString& rhs) const	{	return hashValue() > rhs.hashValue();	}
bool FixString::operator< (const FixString& rhs) const	{	return hashValue() < rhs.hashValue();	}
