#ifndef __roStringHash_h__
#define __roStringHash_h__

#include "roStringUtility.h"

namespace ro {

typedef roUint32 StringHash;

namespace stringHashDetail
{
	/// A small utility for defining the "const char (&Type)[N]" argument
	template<StringHash N> struct Str { typedef const char (&Type)[N]; };

	template<StringHash N> roFORCEINLINE StringHash sdbm(typename Str<N>::Type str)
	{
		// hash(i) = hash(i - 1) * 65599 + str[i]
		// Reference: http://www.cse.yorku.ca/~oz/hash.html sdbm
		return sdbm<N-1>((typename Str<N-1>::Type)str) * 65599 + str[N-2];
	}

	// "1" = char[2]
	// NOTE: Template specialization have to be preformed in namespace scope.
	template<> roFORCEINLINE StringHash sdbm<2>(Str<2>::Type str) { return str[0]; }
}	// namespace stringHashDetail

roFORCEINLINE StringHash stringHash(const char (&str)[2])	{ return stringHashDetail::sdbm<2>(str); }
roFORCEINLINE StringHash stringHash(const char (&str)[3])	{ return stringHashDetail::sdbm<3>(str); }
roFORCEINLINE StringHash stringHash(const char (&str)[4])	{ return stringHashDetail::sdbm<4>(str); }
roFORCEINLINE StringHash stringHash(const char (&str)[5])	{ return stringHashDetail::sdbm<5>(str); }
roFORCEINLINE StringHash stringHash(const char (&str)[6])	{ return stringHashDetail::sdbm<6>(str); }
roFORCEINLINE StringHash stringHash(const char (&str)[7])	{ return stringHashDetail::sdbm<7>(str); }
roFORCEINLINE StringHash stringHash(const char (&str)[8])	{ return stringHashDetail::sdbm<8>(str); }
roFORCEINLINE StringHash stringHash(const char (&str)[9])	{ return stringHashDetail::sdbm<9>(str); }
roFORCEINLINE StringHash stringHash(const char (&str)[10])	{ return stringHashDetail::sdbm<10>(str); }
roFORCEINLINE StringHash stringHash(const char (&str)[11])	{ return stringHashDetail::sdbm<11>(str); }
roFORCEINLINE StringHash stringHash(const char (&str)[12])	{ return stringHashDetail::sdbm<12>(str); }
roFORCEINLINE StringHash stringHash(const char (&str)[13])	{ return stringHashDetail::sdbm<13>(str); }
roFORCEINLINE StringHash stringHash(const char (&str)[14])	{ return stringHashDetail::sdbm<14>(str); }
roFORCEINLINE StringHash stringHash(const char (&str)[15])	{ return stringHashDetail::sdbm<15>(str); }
roFORCEINLINE StringHash stringHash(const char (&str)[16])	{ return stringHashDetail::sdbm<16>(str); }
roFORCEINLINE StringHash stringHash(const char (&str)[17])	{ return stringHashDetail::sdbm<17>(str); }
roFORCEINLINE StringHash stringHash(const char (&str)[18])	{ return stringHashDetail::sdbm<18>(str); }
roFORCEINLINE StringHash stringHash(const char (&str)[19])	{ return stringHashDetail::sdbm<19>(str); }
roFORCEINLINE StringHash stringHash(const char (&str)[20])	{ return stringHashDetail::sdbm<20>(str); }
roFORCEINLINE StringHash stringHash(const char (&str)[21])	{ return stringHashDetail::sdbm<21>(str); }
roFORCEINLINE StringHash stringHash(const char (&str)[22])	{ return stringHashDetail::sdbm<22>(str); }

inline StringHash stringHash(const char* str, roSize len)
{
	StringHash hash = 0;
	len = (len == 0) ? roSize(-1) : len;

	for(roSize i=0; i<len && str[i] != '\0'; ++i) {
		//hash = hash * 65599 + str[i];
		hash = str[i] + (hash << 6) + (hash << 16) - hash;
	}
	return hash;
}

inline StringHash stringLowerCaseHash(const char* str, roSize len=-1)
{
	StringHash hash = 0;
	len = (len == 0) ? roSize(-1) : len;

	for(roSize i=0; i<len && str[i] != '\0'; ++i) {
		//hash = hash * 65599 + roToLower(str[i]);
		hash = roToLower(str[i]) + (hash << 6) + (hash << 16) - hash;
	}
	return hash;
}

}	// namespace ro

#endif	// __roStringHash_h__
