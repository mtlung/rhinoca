#ifndef __roMemory_h__
#define __roMemory_h__

#include "roBytePtr.h"

#ifndef _NEW_
// Define our own placement new and delete operator such that we need not to include <new>
inline void* operator new(size_t, void* where) { return where; }
inline void operator delete(void*, void*) {}
#endif	// _NEW_

roBytePtr	roMalloc(roSize size);
roBytePtr	roRealloc(void* originalPtr, roSize originalSize, roSize newSize);
void		roFree(void* ptr);

struct roDefaultAllocator
{
	roBytePtr malloc(roSize size);
	roBytePtr realloc(void* originalPtr, roSize originalSize, roSize newSize);
	void free(void* ptr);

	template<class T>
	T* newObj()																			{ return new( malloc(sizeof(T)) ) T(); }
	template<class T,class P1>
	T* newObj(const P1&p1)																{ return new( malloc(sizeof(T)) ) T(p1); }
	template<class T,class P1,class P2>
	T* newObj(const P1&p1,const P2&p2)													{ return new( malloc(sizeof(T)) ) T(p1,p2); }
	template<class T,class P1,class P2,class P3>
	T* newObj(const P1&p1,const P2&p2,const P3&p3)										{ return new( malloc(sizeof(T)) ) T(p1,p2,p3); }
	template<class T,class P1,class P2,class P3,class P4>
	T* newObj(const P1&p1,const P2&p2,const P3&p3,const P4&p4)							{ return new( malloc(sizeof(T)) ) T(p1,p2,p3,p4); }
	template<class T,class P1,class P2,class P3,class P4,class P5>
	T* newObj(const P1&p1,const P2&p2,const P3&p3,const P4&p4,const P5&p5)				{ return new( malloc(sizeof(T)) ) T(p1,p2,p3,p4,p5); }
	template<class T,class P1,class P2,class P3,class P4,class P5,class P6>
	T* newObj(const P1&p1,const P2&p2,const P3&p3,const P4&p4,const P5&p5,const P6&p6)	{ return new( malloc(sizeof(T)) ) T(p1,p2,p3,p4,p5,p6); }

	template<class T>
	void deleteObj(T* ptr) { ptr->~T(); free(ptr); }
};

#endif	// __roMemory_h__
