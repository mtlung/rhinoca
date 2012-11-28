#ifndef __roWeakPtr_h__
#define __roWeakPtr_h__

#include "../platform/roCompiler.h"

namespace ro {

/// Not thread safe
template<class T>
struct WeakPtr
{
	typedef WeakPtr<T> this_type;

	WeakPtr()										{ _ptr = NULL; _prev = NULL; _next = NULL; }
	~WeakPtr()										{ _resetLink(); }

	template<class U>
	WeakPtr(const WeakPtr<U>& rhs)					{ _setPtr(rhs._ptr); }
	WeakPtr(const WeakPtr& rhs)						{ _setPtr(rhs._ptr); }
	WeakPtr(T* p)									{ _setPtr(p); }

	template<class U>
	WeakPtr&	operator=(const WeakPtr<U>& rhs)	{ _resetLink(); _setPtr(rhs._ptr); return *this; }
	WeakPtr&	operator=(const WeakPtr& rhs)		{ _resetLink(); _setPtr(rhs._ptr); return *this; }
	WeakPtr&	operator=(T* rhs)					{ _resetLink(); _setPtr(rhs); return *this; }

				operator T* ()						{ return _ptr; }
				operator const T* () const			{ return _ptr; }
	T&			operator*()							{ return *_ptr; }
	T*			operator->()						{ return _ptr; }
	const T&	operator*() const					{ return *_ptr; }
	const T*	operator->() const					{ return _ptr; }

	typedef T* this_type::*unspecified_bool_type;
	bool operator!() const							{ return _ptr == NULL; }	///< Null test
	operator unspecified_bool_type() const			{ return _ptr == NULL ? NULL : &this_type::_ptr; }	///< Non-null test

// Private
	void _resetLink()								{ if(_prev) _prev->_next = _next; if(_next) _next->_prev = _prev; }
	void _setPtr(T* p);

	T* _ptr;
	WeakPtr<T>* _prev;
	WeakPtr<T>* _next;
};	// WeakPtr

template<class T, class U>	bool operator==(const WeakPtr<T>& a, U* b)					{ return a._ptr == b; }
template<class T, class U>	bool operator!=(const WeakPtr<T>& a, U* b)					{ return a._ptr != b; }
template<class T, class U>	bool operator==(T* a, const WeakPtr<U>& b)					{ return a == b._ptr; }
template<class T, class U>	bool operator!=(T* a, const WeakPtr<U>& b)					{ return a != b._ptr; }
template<class T, class U>	bool operator==(const WeakPtr<T>& a, const WeakPtr<U>& b)	{ return a._ptr == b._ptr; }
template<class T, class U>	bool operator!=(const WeakPtr<T>& a, const WeakPtr<U>& b)	{ return a._ptr != b._ptr; }
template<class T, class U>	bool operator<(const WeakPtr<T>& a, const WeakPtr<U>& b)	{ return a._ptr < b._ptr; }

template<class T, class U>	WeakPtr<T> static_pointer_cast(const WeakPtr<U>& p)			{ return static_cast<T*>(p._ptr); }
template<class T, class U>	WeakPtr<T> const_pointer_cast(const WeakPtr<U>& p)			{ return const_cast<T*>(p._ptr); }
template<class T, class U>	WeakPtr<T> dynamic_pointer_cast(const WeakPtr<U>& p)		{ return dynamic_cast<T*>(p._ptr); }


// ----------------------------------------------------------------------

/// A handy class for you to inherit from, in order to use WeakPtr painlessly.
struct WeakObject
{
	WeakObject() {}
	WeakObject(const WeakObject& rhs) {}
	~WeakObject();

	WeakObject& operator=(const WeakObject& rhs) { return *this; }

	WeakPtr<WeakObject> _ptrHead;
};	// WeakObject

template<class T>
void WeakPtr<T>::_setPtr(T* p)
{
	_ptr = p;
	_next = NULL;
	_prev = NULL;

	if(WeakObject* w = static_cast<WeakObject*>(p)) {
		WeakPtr<T>& head = reinterpret_cast<WeakPtr<T>&>(w->_ptrHead);
		_next = head._next;
		_prev = &head;
		head._next = this;
		if(_next)
			_next->_prev = this;
	}
}

inline WeakObject::~WeakObject()
{
	WeakPtr<WeakObject>* head = _ptrHead._next;
	while(head) {
		WeakPtr<WeakObject>* next = head->_next;
		head->_ptr = NULL;
		head->_prev = NULL;
		head->_next = NULL;
		head = next;
	}
}

}	// namespace ro

#endif	// __roWeakPtr_h__
