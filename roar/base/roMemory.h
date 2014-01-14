#ifndef __roMemory_h__
#define __roMemory_h__

#include "roBytePtr.h"
#include "roNonCopyable.h"

#ifndef _NEW_
#define __PLACEMENT_NEW_INLINE
// Define our own placement new and delete operator such that we need not to include <new>
inline void* operator new(size_t, void* where) { return where; }
inline void operator delete(void*, void*) {}
#define _NEW_
#endif	// _NEW_

roBytePtr	roMalloc(roSize size);
roBytePtr	roRealloc(void* originalPtr, roSize originalSize, roSize newSize);
void		roFree(void* ptr);

namespace ro {

struct DefaultAllocator;
extern DefaultAllocator defaultAllocator;

template<class T>
struct AutoPtr
{
	AutoPtr()								: _p(NULL), _allocator(NULL) {}
	AutoPtr(T* p, DefaultAllocator& a)		: _p(p), _allocator(&a) {}
	AutoPtr(const AutoPtr& rhs)				: _p(const_cast<AutoPtr&>(rhs).unref()), _allocator(rhs._allocator) {}
	~AutoPtr()								{ deleteObject(); }

	void		ref(T* p)					{ deleteObject(); _p = p; }

	template<class U>
	void		takeOver(AutoPtr<U>& rhs)	{ ref(static_cast<T*>(rhs._p)); rhs.unref(); _allocator = rhs._allocator; }

	T*			unref()						{ T* t = _p; _p = NULL; return t; }
	void		deleteObject();

	T*			ptr()						{ return  _p; }
	const T*	ptr() const					{ return  _p; }

	T*			operator->()				{ return  _p; }
	const T*	operator->() const			{ return  _p; }
	T&			operator*()					{ return *_p; }
	const T&	operator*() const			{ return *_p; }

	T* _p;
	DefaultAllocator* _allocator;

private:
	void operator=(const AutoPtr&);
};

struct DefaultAllocator
{
	roBytePtr malloc(roSize size);
	roBytePtr realloc(void* originalPtr, roSize originalSize, roSize newSize);
	void free(void* ptr);

	template<class T> T* typedRealloc(T* originalPtr, roSize originalCount, roSize newCount)	{ return realloc(originalPtr, originalCount * sizeof(T), newCount * sizeof(T)).template cast<T>(); }

	template<class T>
	AutoPtr<T> newObj()																			{ return AutoPtr<T>(new( malloc(sizeof(T)) ) T(), *this); }
	template<class T,class P1>
	AutoPtr<T> newObj(const P1&p1)																{ return AutoPtr<T>(new( malloc(sizeof(T)) ) T(p1), *this); }
	template<class T,class P1,class P2>
	AutoPtr<T> newObj(const P1&p1,const P2&p2)													{ return AutoPtr<T>(new( malloc(sizeof(T)) ) T(p1,p2), *this); }
	template<class T,class P1,class P2,class P3>
	AutoPtr<T> newObj(const P1&p1,const P2&p2,const P3&p3)										{ return AutoPtr<T>(new( malloc(sizeof(T)) ) T(p1,p2,p3), *this); }
	template<class T,class P1,class P2,class P3,class P4>
	AutoPtr<T> newObj(const P1&p1,const P2&p2,const P3&p3,const P4&p4)							{ return AutoPtr<T>(new( malloc(sizeof(T)) ) T(p1,p2,p3,p4), *this); }
	template<class T,class P1,class P2,class P3,class P4,class P5>
	AutoPtr<T> newObj(const P1&p1,const P2&p2,const P3&p3,const P4&p4,const P5&p5)				{ return AutoPtr<T>(new( malloc(sizeof(T)) ) T(p1,p2,p3,p4,p5), *this); }
	template<class T,class P1,class P2,class P3,class P4,class P5,class P6>
	AutoPtr<T> newObj(const P1&p1,const P2&p2,const P3&p3,const P4&p4,const P5&p5,const P6&p6)	{ return AutoPtr<T>(new( malloc(sizeof(T)) ) T(p1,p2,p3,p4,p5,p6), *this); }

	template<class T>
	void deleteObj(T* ptr) { if(ptr) { ptr->~T(); free(ptr); } }
};

template<class T>
void AutoPtr<T>::deleteObject()
{
	if(!_p)
		return;

	roAssert(_allocator);
	if(_allocator)
		_allocator->deleteObj(_p);
	_p = NULL;
}

}	// namespace ro

template<class T>
void roSwap(ro::AutoPtr<T>& lhs, ro::AutoPtr<T>& rhs)
{
	T* tmpp = lhs._p;
	lhs._p = rhs._p;
	rhs._p = tmpp;

	ro::DefaultAllocator* tmpa = lhs._allocator;
	lhs._allocator = rhs._allocator;
	rhs._allocator = tmpa;
}

#endif	// __roMemory_h__
