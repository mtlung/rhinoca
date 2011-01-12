#ifndef __INTRUSIVEPTR_H__
#define __INTRUSIVEPTR_H__

#include "Platform.h"

/*!	A smart pointer that uses intrusive reference counting.
	Relies on unqualified calls to
	void intrusivePtrAddRef(T* p);
	void intrusivePtrRelease(T* p);

	It is worth to note that IntrusivePtr made no assumption on the memory allocation/de-allocation
	of the pointee type. That means user are free to use their own memory allocation scheme.

	\note The object is responsible for destroying itself (eg. inside the intrusivePtrRelease() function).
	\note IntrusivePtr is thread safe if operation in intrusivePtrAddRef() and intrusivePtrRelease()
		  are thread safe (eg. use atomic integer for the reference counter)
 */
template<class T>
class IntrusivePtr
{
	typedef IntrusivePtr<T> this_type;

public:
	typedef T* Pointer;

	IntrusivePtr() : _ptr(NULL)	{}

	IntrusivePtr(T* p, bool addRef=true)
		: _ptr(p)
	{
		if(_ptr != NULL && addRef)
			intrusivePtrAddRef(_ptr);
	}

	template<class U>
	IntrusivePtr(const IntrusivePtr<U>& rhs)
		: _ptr(rhs.get())
	{
		if(_ptr != NULL)
			intrusivePtrAddRef(_ptr);
	}

	IntrusivePtr(const IntrusivePtr& rhs)
		: _ptr(rhs.get())
	{
		if(_ptr != NULL)
			intrusivePtrAddRef(_ptr);
	}

	~IntrusivePtr()
	{
		if(_ptr != NULL)
			intrusivePtrRelease(_ptr);
	}

	template<class U>
	IntrusivePtr& operator=(const IntrusivePtr<U>& rhs)
	{
		this_type(rhs).swap(*this);
		return *this;
	}

	IntrusivePtr& operator=(const IntrusivePtr& rhs)
	{
		this_type(rhs).swap(*this);
		return *this;
	}

	IntrusivePtr& operator=(T* rhs)
	{
		this_type(rhs).swap(*this);
		return *this;
	}

	T* get() const {
		return _ptr;
	}

	T& operator*() const {
		return *_ptr;
	}

	T* operator->() const {
		return _ptr;
	}

	typedef T* this_type::*unspecified_bool_type;

	//!	Non-Null test for using "if (p) ..." to check whether p is NULL.
	operator unspecified_bool_type() const {
		return _ptr == NULL ? NULL : &this_type::_ptr;
	}

	//!	Null test for using "if(!p) ..." to check whether p is NULL.
	bool operator!() const {
		return _ptr == NULL;
	}

	void swap(IntrusivePtr& rhs)
	{
		T* tmp = _ptr;
		_ptr = rhs._ptr;
		rhs._ptr = tmp;
	}

private:
	T* _ptr;
};	// IntrusivePtr

template<class T, class U> inline
bool operator==(const IntrusivePtr<T>& a, const IntrusivePtr<U>& b) {
	return a.get() == b.get();
}

template<class T, class U> inline
bool operator!=(const IntrusivePtr<T>& a, const IntrusivePtr<U>& b) {
	return a.get() != b.get();
}

template<class T, class U> inline
bool operator<(const IntrusivePtr<T>& a, const IntrusivePtr<U>& b) {
	return a.get() < b.get();
}

template<class T, class U> inline
bool operator==(const IntrusivePtr<T>& a, U* b) {
	return a.get() == b;
}

template<class T, class U> inline
bool operator!=(const IntrusivePtr<T>& a, U* b) {
	return a.get() != b;
}

template<class T, class U> inline
bool operator==(T* a, const IntrusivePtr<U>& b) {
	return a == b.get();
}

template<class T, class U> inline
bool operator!=(T* a, const IntrusivePtr<U>& b) {
	return a != b.get();
}

template<class T> inline
void swap(IntrusivePtr<T>& lhs, IntrusivePtr<T>& rhs) {
	lhs.swap(rhs);
}

template<class T, class U>
IntrusivePtr<T> static_pointer_cast(const IntrusivePtr<U>& p) {
	return static_cast<T*>(p.get());
}

template<class T, class U>
IntrusivePtr<T> const_pointer_cast(const IntrusivePtr<U>& p) {
	return const_cast<T*>(p.get());
}

template<class T, class U>
IntrusivePtr<T> dynamic_pointer_cast(const IntrusivePtr<U>& p) {
	return dynamic_cast<T*>(p.get());
}

/*!	A handly class for you to inherit from, in order to uss IntrusivePtr painlessly.
	\note Inherit from this class will make your class polymorhpic, don't use this
		class if you don't want to pay for this overhead.
 */
template<typename CounterType>
class IntrusiveSharedObject
{
public:
	IntrusiveSharedObject() : _refCount(0) {}

	virtual ~IntrusiveSharedObject() {}

	friend void intrusivePtrAddRef(IntrusiveSharedObject* p) {
		++(p->_refCount);
	}

	friend void intrusivePtrRelease(IntrusiveSharedObject* p) {
		// NOTE: Gcc4.2 failed to compile "--(p->_refCount)" correctly.
		p->_refCount--;
		if(p->_refCount == 0)
			delete p;
	}

protected:
	mutable CounterType _refCount;
};	// IntrusiveSharedObject

#endif	// __INTRUSIVEPTR_H__
