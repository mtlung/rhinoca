#ifndef __SharedPtr_H__
#define __SharedPtr_H__

/*!	A smart pointer that uses intrusive reference counting.
	Relies on unqualified calls to
	void SharedPtrAddRef(T* p);
	void SharedPtrRelease(T* p);

	It is worth to note that SharedPtr made no assumption on the memory allocation/de-allocation
	of the pointee type. That means user are free to use their own memory allocation scheme.

	\note The object is responsible for destroying itself (eg. inside the SharedPtrRelease() function).
	\note SharedPtr is thread safe if operation in SharedPtrAddRef() and SharedPtrRelease()
		  are thread safe (eg. use atomic integer for the reference counter)
 */
template<class T>
class SharedPtr
{
	typedef SharedPtr<T> this_type;

public:
	typedef T* Pointer;

	SharedPtr() : _ptr(NULL)	{}

	SharedPtr(T* p, bool addRef=true)
		: _ptr(p)
	{
		if(_ptr != NULL && addRef)
			SharedPtrAddRef(_ptr);
	}

	template<class U>
	SharedPtr(const SharedPtr<U>& rhs)
		: _ptr(rhs.get())
	{
		if(_ptr != NULL)
			SharedPtrAddRef(_ptr);
	}

	SharedPtr(const SharedPtr& rhs)
		: _ptr(rhs.get())
	{
		if(_ptr != NULL)
			SharedPtrAddRef(_ptr);
	}

	~SharedPtr()
	{
		if(_ptr != NULL)
			SharedPtrRelease(_ptr);
	}

	template<class U>
	SharedPtr& operator=(const SharedPtr<U>& rhs)
	{
		this_type(rhs).swap(*this);
		return *this;
	}

	SharedPtr& operator=(const SharedPtr& rhs)
	{
		this_type(rhs).swap(*this);
		return *this;
	}

	SharedPtr& operator=(T* rhs)
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

	void swap(SharedPtr& rhs)
	{
		T* tmp = _ptr;
		_ptr = rhs._ptr;
		rhs._ptr = tmp;
	}

private:
	T* _ptr;
};	// SharedPtr

template<class T, class U> inline
bool operator==(const SharedPtr<T>& a, const SharedPtr<U>& b) {
	return a.get() == b.get();
}

template<class T, class U> inline
bool operator!=(const SharedPtr<T>& a, const SharedPtr<U>& b) {
	return a.get() != b.get();
}

template<class T, class U> inline
bool operator<(const SharedPtr<T>& a, const SharedPtr<U>& b) {
	return a.get() < b.get();
}

template<class T, class U> inline
bool operator==(const SharedPtr<T>& a, U* b) {
	return a.get() == b;
}

template<class T, class U> inline
bool operator!=(const SharedPtr<T>& a, U* b) {
	return a.get() != b;
}

template<class T, class U> inline
bool operator==(T* a, const SharedPtr<U>& b) {
	return a == b.get();
}

template<class T, class U> inline
bool operator!=(T* a, const SharedPtr<U>& b) {
	return a != b.get();
}

template<class T> inline
void swap(SharedPtr<T>& lhs, SharedPtr<T>& rhs) {
	lhs.swap(rhs);
}

template<class T, class U>
SharedPtr<T> static_pointer_cast(const SharedPtr<U>& p) {
	return static_cast<T*>(p.get());
}

template<class T, class U>
SharedPtr<T> const_pointer_cast(const SharedPtr<U>& p) {
	return const_cast<T*>(p.get());
}

template<class T, class U>
SharedPtr<T> dynamic_pointer_cast(const SharedPtr<U>& p) {
	return dynamic_cast<T*>(p.get());
}

/*!	A handy class for you to inherit from, in order to use SharedPtr painlessly.
	\note Inherit from this class will make your class polymorphic, don't use this
		class if you don't want to pay for this overhead.
 */
template<typename CounterType>
class SharedObject
{
public:
	SharedObject() : _refCount(0) {}

	virtual ~SharedObject() {}

	friend void SharedPtrAddRef(SharedObject* p) {
		++(p->_refCount);
	}

	friend void SharedPtrRelease(SharedObject* p) {
		// NOTE: Gcc4.2 failed to compile "--(p->_refCount)" correctly.
		p->_refCount--;
		if(p->_refCount == 0)
			delete p;
	}

protected:
	mutable CounterType _refCount;
};	// SharedObject

#endif	// __SharedPtr_H__
