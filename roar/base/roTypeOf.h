#ifndef __roTypeOf_h__
#define __roTypeOf_h__

#include "../platform/roCompiler.h"

namespace ro {

template<typename T>	
struct TypeOf {
	static T valueMin();
	static T valueMax();

	static bool isSigned();
	static bool isUnsigned();

#ifdef _MSC_VER
	static bool isPOD() { return __is_pod(T); }
#else
	static bool isPOD() { return false; }	///< Is plain old data type
#endif
};

template<typename T>	
struct TypeOf<T*> {
	static bool isPOD() { return true; }
};

#define roTYPE_LIST(T) \
	template<> inline bool TypeOf<T>::isPOD()		{ return true; } \
	template<> inline bool TypeOf<T>::isSigned()	{ return true; } \
	template<> inline bool TypeOf<T>::isUnsigned()	{ return false; } \

roTYPE_LIST(float);
roTYPE_LIST(double);
roTYPE_LIST(long);
roTYPE_LIST(roInt8);
roTYPE_LIST(roInt16);
roTYPE_LIST(roInt32);
roTYPE_LIST(roInt64);

#undef roTYPE_LIST

#define roTYPE_LIST(T) \
	template<> inline bool TypeOf<T>::isPOD()		{ return true; } \
	template<> inline bool TypeOf<T>::isSigned()	{ return false; } \
	template<> inline bool TypeOf<T>::isUnsigned()	{ return true; } \

roTYPE_LIST(unsigned long);
roTYPE_LIST(roUint8);
roTYPE_LIST(roUint16);
roTYPE_LIST(roUint32);
roTYPE_LIST(roUint64);

#undef roTYPE_LIST

template<> inline float		TypeOf<float>::valueMin()	{ return 1.175494351e-38F; }
template<> inline float		TypeOf<float>::valueMax()	{ return 3.402823466e+38F; }

template<> inline double	TypeOf<double>::valueMin()	{ return 2.2250738585072014e-308; }
template<> inline double	TypeOf<double>::valueMax()	{ return 1.7976931348623158e+308; }

template<> inline roInt8	TypeOf<roInt8>::valueMin()	{ return (-0x7f-1); }
template<> inline roInt8	TypeOf<roInt8>::valueMax()	{ return 0x7f; }

template<> inline roInt16	TypeOf<roInt16>::valueMin()	{ return (-0x7fff-1); }
template<> inline roInt16	TypeOf<roInt16>::valueMax()	{ return 0x7fff; }

template<> inline roInt32	TypeOf<roInt32>::valueMin()	{ return (-0x7fffffff-1); }
template<> inline roInt32	TypeOf<roInt32>::valueMax()	{ return 0x7fffffff; }

template<> inline roInt64	TypeOf<roInt64>::valueMin()	{ return (-0x7fffffffffffffffLL-1); }
template<> inline roInt64	TypeOf<roInt64>::valueMax()	{ return 0x7fffffffffffffffLL; }

template<> inline roUint8	TypeOf<roUint8>::valueMin()	{ return 0; }
template<> inline roUint8	TypeOf<roUint8>::valueMax()	{ return 0xff; }

template<> inline roUint16	TypeOf<roUint16>::valueMin(){ return 0; }
template<> inline roUint16	TypeOf<roUint16>::valueMax(){ return 0xffff; }

template<> inline roUint32	TypeOf<roUint32>::valueMin(){ return 0; }
template<> inline roUint32	TypeOf<roUint32>::valueMax(){ return 0xffffffffU; }

template<> inline roUint64	TypeOf<roUint64>::valueMin(){ return 0; }
template<> inline roUint64	TypeOf<roUint64>::valueMax(){ return 0xffffffffffffffffULL; }

template<class T> struct TypeHolder {};

// Signed/unsigned counter part
template<typename T> struct SignedCounterPart	{ typedef T Ret;		};
template<>	struct SignedCounterPart<roUint8>	{ typedef roInt8 Ret;	};
template<>	struct SignedCounterPart<roUint16>	{ typedef roInt16 Ret;	};
template<>	struct SignedCounterPart<roUint32>	{ typedef roInt32 Ret;	};
template<>	struct SignedCounterPart<roUint64>	{ typedef roInt64 Ret;	};

template<typename T> struct UnsignedCounterPart	{ typedef T Ret;		};
template<>	struct UnsignedCounterPart<roInt8>	{ typedef roUint8 Ret;	};
template<>	struct UnsignedCounterPart<roInt16>	{ typedef roUint16 Ret;	};
template<>	struct UnsignedCounterPart<roInt32>	{ typedef roUint32 Ret;	};
template<>	struct UnsignedCounterPart<roInt64>	{ typedef roUint64 Ret;	};

}	// namespace ro

#endif	// __roTypeOf_h__
