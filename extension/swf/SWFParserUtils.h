#ifndef __SWFParserUtils__h__
#define __SWFParserUtils__h__

#include "SWFDefines.h"

//------------------------------------------------------------------------------
template<typename T>
T ReadData(const char * buffer, long offset)
{
	T value = * (T *) (buffer + offset);
	return value;
}

//------------------------------------------------------------------------------
bool ReadBit( const UI8 data, unsigned char position );
const char * TagTypeToString(int tag);
int	ReadRECT(RECT * rect, char * buffer);
int ReadTagCodeAndLength(char * buffer, long offset, unsigned short * type, unsigned int * length);


//------------------------------------------------------------------------------
#endif