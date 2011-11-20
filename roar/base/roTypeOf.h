#ifndef __roTypeOf_h__
#define __roTypeOf_h__

namespace ro {

template<typename T>	
struct TypeOf {
	static T valueMin();
	static T valueMax();

	static bool isUnsigned();

#ifdef _MSC_VER
	static bool isPOD() { return __is_pod(T); }
#else
	static bool isPOD() { return false; }	//! Is plain old data type
#endif
};

#define roTYPE_LIST(T) \
	template<> bool TypeOf<T>::isPOD()		{ return true; } \
	template<> bool TypeOf<T>::isUnsigned()	{ return false; } \

roTYPE_LIST(float);
roTYPE_LIST(double);
roTYPE_LIST(roInt8);
roTYPE_LIST(roInt16);
roTYPE_LIST(roInt32);
roTYPE_LIST(roInt64);

#undef roTYPE_LIST

#define roTYPE_LIST(T) \
	template<> bool TypeOf<T>::isPOD()		{ return true; } \
	template<> bool TypeOf<T>::isUnsigned()	{ return true; } \

roTYPE_LIST(roUint8);
roTYPE_LIST(roUint16);
roTYPE_LIST(roUint32);
roTYPE_LIST(roUint64);

#undef roTYPE_LIST

template<> float	TypeOf<float>::valueMin()		{ return 1.175494351e-38F; }
template<> float	TypeOf<float>::valueMax()		{ return 3.402823466e+38F; }

template<> double	TypeOf<double>::valueMin()	{ return 2.2250738585072014e-308; }
template<> double	TypeOf<double>::valueMax()	{ return 1.7976931348623158e+308; }

template<> roInt8	TypeOf<roInt8>::valueMin()	{ return (-0x7f-1); }
template<> roInt8	TypeOf<roInt8>::valueMax()	{ return 0x7f; }

template<> roInt16	TypeOf<roInt16>::valueMin()	{ return (-0x7fff-1); }
template<> roInt16	TypeOf<roInt16>::valueMax()	{ return 0x7fff; }

template<> roInt32	TypeOf<roInt32>::valueMin()	{ return (-0x7fffffff-1); }
template<> roInt32	TypeOf<roInt32>::valueMax()	{ return 0x7fffffff; }

template<> roInt64	TypeOf<roInt64>::valueMin()	{ return (-0x7fffffffffffffffLL-1); }
template<> roInt64	TypeOf<roInt64>::valueMax()	{ return 0x7fffffffffffffffLL; }

template<> roUint8	TypeOf<roUint8>::valueMin()	{ return 0; }
template<> roUint8	TypeOf<roUint8>::valueMax()	{ return 0xff; }

template<> roUint16	TypeOf<roUint16>::valueMin()	{ return 0; }
template<> roUint16	TypeOf<roUint16>::valueMax()	{ return 0xffff; }

template<> roUint32	TypeOf<roUint32>::valueMin()	{ return 0; }
template<> roUint32	TypeOf<roUint32>::valueMax()	{ return 0xffffffffU; }

template<> roUint64	TypeOf<roUint64>::valueMin()	{ return 0; }
template<> roUint64	TypeOf<roUint64>::valueMax()	{ return 0xffffffffffffffffULL; }

template<class T> struct TypeHolder {};

}	// namespace ro

#endif	// __roTypeOf_h__
