#ifndef __roSharedPtr_h__
#define __roSharedPtr_h__

namespace ro {

/// A smart pointer that uses intrusive reference counting.
/// Relies on unqualified calls to
/// void sharedPtrAddRef(T* p);
/// void sharedPtrRelease(T* p);
/// 
/// It is worth to note that SharedPtr made no assumption on the memory allocation/de-allocation
/// of the pointee type. That means user are free to use their own memory allocation scheme.
/// 
/// \note The object is responsible for destroying itself (eg. inside the SharedPtrRelease() function).
/// \note SharedPtr is thread safe if operation in SharedPtrAddRef() and SharedPtrRelease()
/// 	  are thread safe (eg. use atomic integer for the reference counter)
template<class T>
struct SharedPtr
{
	typedef SharedPtr<T> this_type;

	SharedPtr() : _ptr(NULL)	{}
	~SharedPtr() { if(_ptr) sharedPtrRelease(_ptr); }

	template<class U>
	SharedPtr(const SharedPtr<U>& rhs) : _ptr(rhs.get()) { if(_ptr) sharedPtrAddRef(_ptr); }
	SharedPtr(const SharedPtr& rhs) : _ptr(rhs.get()) { if(_ptr) sharedPtrAddRef(_ptr); }
	SharedPtr(T* p, bool addRef=true) : _ptr(p) { if(_ptr && addRef) sharedPtrAddRef(_ptr); }

	template<class U>
	SharedPtr& operator=(const SharedPtr<U>& rhs)	{ this_type(rhs).swap(*this); return *this; }
	SharedPtr& operator=(const SharedPtr& rhs)		{ this_type(rhs).swap(*this); return *this; }
	SharedPtr& operator=(T* rhs)					{ this_type(rhs).swap(*this); return *this; }

	T*	get() const			{ return _ptr; }
	T&	operator*() const	{ return *_ptr; }
	T*	operator->() const	{ return _ptr; }

	typedef T* this_type::*unspecified_bool_type;
	bool operator!() const { return _ptr == NULL; }	///< Null test
	operator unspecified_bool_type() const { return _ptr == NULL ? NULL : &this_type::_ptr; }	///< Non-null test

	void swap(SharedPtr& rhs) { T* tmp(_ptr); _ptr = rhs._ptr; rhs._ptr = tmp; }

// Private
	T* _ptr;
};	// SharedPtr

template<class T, class U>	bool operator==(const SharedPtr<T>& a, U* b) { return a.get() == b; }
template<class T, class U>	bool operator!=(const SharedPtr<T>& a, U* b) { return a.get() != b; }
template<class T, class U>	bool operator==(T* a, const SharedPtr<U>& b) { return a == b.get(); }
template<class T, class U>	bool operator!=(T* a, const SharedPtr<U>& b) { return a != b.get(); }
template<class T, class U>	bool operator==(const SharedPtr<T>& a, const SharedPtr<U>& b)	{ return a.get() == b.get(); }
template<class T, class U>	bool operator!=(const SharedPtr<T>& a, const SharedPtr<U>& b)	{ return a.get() != b.get(); }
template<class T, class U>	bool operator<(const SharedPtr<T>& a, const SharedPtr<U>& b)	{ return a.get() < b.get(); }

template<class T, class U>	SharedPtr<T> static_pointer_cast(const SharedPtr<U>& p)	{ return static_cast<T*>(p.get()); }
template<class T, class U>	SharedPtr<T> const_pointer_cast(const SharedPtr<U>& p)	{ return const_cast<T*>(p.get()); }
template<class T, class U>	SharedPtr<T> dynamic_pointer_cast(const SharedPtr<U>& p){ return dynamic_cast<T*>(p.get()); }


// ----------------------------------------------------------------------

/// A handy class for you to inherit from, in order to use SharedPtr painlessly.
template<class CounterType>
struct SharedObject
{
	SharedObject() : _refCount(0) {}
	virtual ~SharedObject() {}

	friend void sharedPtrAddRef(SharedObject* p) {
		++(p->_refCount);
	}

	friend void sharedPtrRelease(SharedObject* p) {
		// NOTE: Gcc4.2 failed to compile "--(p->_refCount)" correctly.
		p->_refCount--;
		if(p->_refCount == 0)
			delete p;
	}

	mutable CounterType _refCount;
};	// SharedObject

}	// namespace ro

template<class T> void roSwap(ro::SharedPtr<T>& lhs, ro::SharedPtr<T>& rhs) { lhs.swap(rhs); }

#endif	// __roSharedPtr_h__
