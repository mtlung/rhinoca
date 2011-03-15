#ifndef __SWFParserUtils__h__
#define __SWFParserUtils__h__

#include "SWFDefines.h"


class ParserUtils
{
public:
	static int	ReadMatrix(const char *data);
};

//------------------------------------------------------------------------------
class BitReader
{
public:
	// return the bits readed so far
	static unsigned ReadBoolBit( 
									bool & result
								,	const char * buffer
								,	unsigned offsetInBits
								);
	static unsigned ReadIntBits(  long & result
							,	const char * buffer
							,	unsigned offsetInBits
							,	unsigned bitCount								
							);	

	// Fix Point Number is either 16.16 or 8.8 as specified in 
	// P.12 of SWF spec
	static unsigned ReadFixedBits(  float & result
							,	const char * buffer
							,	unsigned offsetInBits
							,	unsigned bitCount								
							);	
};

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
int	ReadRECT(RECT * rect, const char * buffer);
int ReadTagCodeAndLength(char * buffer, long offset, unsigned short * type, unsigned int * length);

//------------------------------------------------------------------------------
#endif