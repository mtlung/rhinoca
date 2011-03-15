#include "pch.h"
#include "SWFParserUtils.h"

#include <cassert>
#include <map>
#include <math.h>
//------------------------------------------------------------------------------
unsigned BitReader::ReadBoolBit( bool & result, const char * buffer, unsigned offsetInBits)
{
	int byteSkip	= offsetInBits / 8;
	int posInByte	= 7 - offsetInBits % 8;
	assert( 0<=posInByte && posInByte<8);

	const char * data = (buffer+byteSkip);
	unsigned bitReaded = 0;
	result = 0;
	UI8 maskes[] = { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };	
		
	UI8 mask = maskes[posInByte];		
	char value = *data;
	unsigned int bit = (*data) & mask;
	bit = bit >> posInByte;
	assert( bit == 0 || bit == 1 );
	result = bit;
	return 1;
}

//------------------------------------------------------------------------------
unsigned BitReader::ReadIntBits(  long & result
								,	const char * buffer
								,	unsigned offsetInBits
								,	unsigned bitCount								
								)
{

	char firstByte = *buffer;
	char secondBute = *(buffer+1);

	assert( bitCount > 0 );
	if( bitCount == 0 )
	{
		result = 0;
		return 0;
	}

	int byteSkip	= offsetInBits / 8;
	int posInByte	= 7 - offsetInBits % 8;
	assert( 0<=posInByte && posInByte<8);

	const char * data = (buffer+byteSkip);
	unsigned bitReaded = 0;
	result = 0;
	UI8 maskes[] = { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };	
		
	bool isNegative = false;

	UI8 mask = maskes[posInByte];				
	unsigned int bit = (*data) & mask;
	bit = bit >> posInByte;
	if( 1 == bit )
		isNegative = true;
		
	-- posInByte;
	if( posInByte<0)
	{
		posInByte = 7;
		++data;
	}

	bitReaded = 1;
		
	for(int j=1;j<bitCount;++j)
	{
		UI8 mask = maskes[posInByte];				
		unsigned int bit = (*data) & mask;
		bit = bit >> posInByte;
		assert( bit>=0 && bit<=1 );
		if( isNegative )
		{
			bit = ! bit;
		}
		result = (result<<1) | bit;			
		--posInByte;
		if( posInByte < 0 )
		{
			posInByte = 7;
			++ data;
		}
		++bitReaded;
	}
	if( isNegative )
	{
		result += 1;
		result = -result;
	}		
	return bitReaded;
}

//------------------------------------------------------------------------------
// TODO:Not Implemented
//------------------------------------------------------------------------------
unsigned BitReader::ReadFixedBits(  float & result
						,	const char * buffer
						,	unsigned offsetInBits
						,	unsigned bitCount								
						)
{
	assert( 16==bitCount || 32==bitCount );

	unsigned halfCount = bitCount >> 1;

	assert( 8==halfCount || 16==halfCount );

	long integralPart = 0;

	// the upper half is just integral part read it 
	BitReader::ReadIntBits( integralPart, buffer, offsetInBits, halfCount);
	return bitCount;
}

//------------------------------------------------------------------------------
bool ReadBit( const UI8 data, unsigned char position )
{
	assert( position <= 7 );
	UI8 maskes[] = { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };
	UI8 mask = maskes[position];	// 1 << position;
	return data & mask;	
}

/*
//------------------------------------------------------------------------------
const char * TagTypeToString( int tag )
{
	// reference: http://www.adobe.com/content/dam/Adobe/en/devnet/swf/pdf/swf_file_format_spec_v10.pdf
	// p.271
	// notice that some key is missing (e.g. 3) by definition
	static const char* map[] = 
	{
		"End"					// 0
	,	"ShowFrame"				// 1
	,	"DefineShape"			// 2
	,	NULL					// 3 is NULL
	,	"PlaceObject"			// 4
	,	"RemoveObject"			// 5
	,	"DefineBits"			// 6
	,	"DefineButton"			// 7
	,	"JPEGTables"			// 8
	,	"SetBackgroundColor"	// 9
	,	"DefineFont"				// 10
	,	"DefineText"				// 11
	,	"DoAction"					// 12
	,	"DefineFontInfo"			// 13
	,	"DefineSound"				// 14
	,	"StartSound"				// 15
	,	NULL						// 16 is NULL
	,	"DefineButtonSound"			// 17
	,	"SoundStreamHead"			// 18
	,	"SoundStreamBlock"			// 19
	,	"DefineBitsLossless"	// 20
	,	"DefineBitsJPEG2"		// 21
	,	"DefineShape2"			// 22
	,	"DefineButtonCxform"	// 23
	,	"Protect"				// 24
	,	NULL					// 25 is NULL
	,	"PlaceObject2"			// 26
	,	NULL					// 27 is NULL
	,	"RemoveObject2"			// 28
	,	NULL					// 29 is NULL
	,	NULL						// 30 is NULL
	,	NULL						// 31 is NULL
	,	"DefineShape3"				// 32
	,	"DefineText2"				// 33
	,	"DefineButton2"				// 34
	,	"DefineBitsJPEG3"			// 35
	,	"DefineBitsLossless2"		// 36
	,	"DefineEditText"			// 37
	,	NULL						// 38 is NULL
	,	"DefineSprite"				// 39
	,	NULL					// 40 is NULL
	,	NULL					// 41 is NULL
	,	NULL					// 42 is NULL
	,	"FrameLabel"			// 43
	,	NULL					// 44 is NULL
	,	"SoundStreamHead2"		// 45
	,	"DefineMorphShape"		// 46
	,	NULL					// 47 is NULL
	,	"DefineFont2"			// 48
	,	NULL					// 49 is NULL
	,	NULL						// 50 is NULL
	,	NULL						// 51 is NULL
	,	NULL						// 52 is NULL
	,	NULL						// 53 is NULL
	,	NULL						// 54 is NULL
	,	NULL						// 55 is NULL	
	,	"ExportAssets"				// 56
	,	"ImportAssets"				// 57
	,	"EnableDebugger"			// 58
	,	"DoInitAction"				// 59
	,	"DefineVideoStream"	// 60
	,	"VideoFrame"		// 61
	,	"DefineFontInfo2"	// 62
	,	NULL				// 63 is NULL
	,	"EnableDebugger2"	// 64
	,	"ScriptLimits"		// 65
	,	"SetTabIndex"		// 66
	,	NULL				// 67 is NULL
	,	NULL				// 68 is NULL
	,	"FileAttributes"	// 69 
	,	"PlaceObject3"				// 70
	,	"ImportAssets2"				// 71
	,	NULL						// 72 is NULL
	,	"DefineFontAlignZones"		// 73
	,	"CSMTextSettings"			// 74
	,	"DefineFont3"				// 75
	,	"SymbolClass"				// 76
	,	"Metadata"					// 77
	,	"DefineScalingGrid"			// 78
	,	NULL						// 79 is NULL
	,	NULL							// 80 is NULL
	,	NULL							// 81 is NULL
	,	"DoABC"							// 82
	,	"DefineShape4"					// 83
	,	"DefineMorphShape2"				// 84
	,	NULL							// 85 is NULL
	,	"DefineSceneAndFrameLabelData"	// 86
	,	"DefineBinaryData"				// 87
	,	"DefineFontName"				// 88
	,	"StartSound2"					// 89
	,	"DefineBitsJPEG4"	// 90
	,	"DefineFont4"		// 91
	};
	return map[tag];
}
*/

//------------------------------------------------------------------------------
const char * TagTypeToString( int tag )
{
	// reference: http://www.adobe.com/content/dam/Adobe/en/devnet/swf/pdf/swf_file_format_spec_v10.pdf 
	// p.271
	// notice that some key is missing (e.g. 3) by definition
	static std::map<int, const char *> map;
	map[0] = "End";
	map[1] = "ShowFrame";
	map[2] = "DefineShape";
	map[4] = "PlaceObject";
	map[5] = "RemoveObject";
	map[6] = "DefineBits";
	map[7] = "DefineButton";
	map[8] = "JPEGTables";
	map[9] = "SetBackgroundColor";
	map[10] = "DefineFont";
	map[11] = "DefineText";
	map[12] = "DoAction";
	map[13] = "DefineFontInfo";
	map[14] = "DefineSound";
	map[15] = "StartSound";
	map[17] = "DefineButtonSound";
	map[18] = "SoundStreamHead";
	map[19] = "SoundStreamBlock";
	map[20] = "DefineBitsLossless";
	map[21] = "DefineBitsJPEG2";
	map[22] = "DefineShape2";
	map[23] = "DefineButtonCxform";
	map[24] = "Protect";
	map[26] = "PlaceObject2";
	map[28] = "RemoveObject2";
	map[32] = "DefineShape3";
	map[33] = "DefineText2";
	map[34] = "DefineButton2";
	map[35] = "DefineBitsJPEG3";
	map[36] = "DefineBitsLossless2";
	map[37] = "DefineEditText";
	map[39] = "DefineSprite";
	map[43] = "FrameLabel";
	map[45] = "SoundStreamHead2";
	map[46] = "DefineMorphShape";
	map[48] = "DefineFont2";
	map[56] = "ExportAssets";
	map[57] = "ImportAssets";
	map[58] = "EnableDebugger";
	map[59] = "DoInitAction";
	map[60] = "DefineVideoStream";
	map[61] = "VideoFrame";
	map[62] = "DefineFontInfo2";
	map[64] = "EnableDebugger2";
	map[65] = "ScriptLimits";
	map[66] = "SetTabIndex";
	map[69] = "FileAttributes";
	map[70] = "PlaceObject3";
	map[71] = "ImportAssets2";
	map[73] = "DefineFontAlignZones";
	map[74] = "CSMTextSettings";
	map[75] = "DefineFont3";
	map[76] = "SymbolClass";
	map[77] = "Metadata";
	map[78] = "DefineScalingGrid";
	map[82] = "DoABC";
	map[83] = "DefineShape4";
	map[84] = "DefineMorphShape2";
	map[86] = "DefineSceneAndFrameLabelData";
	map[87] = "DefineBinaryData";
	map[88] = "DefineFontName";
	map[89] = "StartSound2";
	map[90] = "DefineBitsJPEG4";
	map[91] = "DefineFont4";
	std::map<int, const char *>::iterator it = map.find( tag );
	if( it != map.end() )
		return (*it).second;
	return NULL;
}

//------------------------------------------------------------------------------
int	ReadRECT( RECT * rect, const char * buffer )
{
	unsigned nBits = (unsigned char) buffer[0];
	unsigned short mask = 255 << (8-5);	
	unsigned short bits = nBits & mask; 
	bits = bits >> (8-5);	
	assert( bits <= 31 );	
	int nByte = (int)ceil( (float)(4 * bits + 5)/ 8 );

	long xMin = 0;
	long xMax = 0;
	long yMin = 0;
	long yMax = 0;
	
	BitReader::ReadIntBits( xMin, buffer, 5 + 0*bits, bits);
	BitReader::ReadIntBits( xMax, buffer, 5 + 1*bits, bits);
	BitReader::ReadIntBits( yMin, buffer, 5 + 2*bits, bits);
	BitReader::ReadIntBits( yMax, buffer, 5 + 3*bits, bits);
		
	rect->xMin = xMin;
	rect->xMax = xMax;
	rect->yMin = yMin;
	rect->yMax = yMax;

	float xMinInPixels = (float)xMin / 20.0f;
	float xMaxInPixels = (float)xMax / 20.0f;
	float yMinInPixels = (float)yMin / 20.0f;
	float yMaxInPixels = (float)yMax / 20.0f;

	return nByte;
}

//------------------------------------------------------------------------------
int ReadTagCodeAndLength( 
							char * buffer
						,	long offset
						,	unsigned short * type
						,	unsigned int * length 
						)
{
	int read = 0;
	unsigned short tagCodeAndLength = 0;
	tagCodeAndLength = ReadData<UI16>(buffer, offset);
	read += sizeof( unsigned short);	

	// the upper 10 bits is the type of the tag
	unsigned short mask = 0xFFFF;
	mask = mask << (16 - 10);
	unsigned short t = tagCodeAndLength & mask;
	*type = t >> (16-10);	

	// the lower 6 bits is the length of the tag
	unsigned short mask2 = mask >> (16-6);
	*length = tagCodeAndLength & mask2;

	return read;
}
//------------------------------------------------------------------------------


// MATRIX RECORD
// reference: http://www.adobe.com/content/dam/Adobe/en/devnet/swf/pdf/swf_file_format_spec_v10.pdf 
// p.20
//------------------------------------------------------------------------------
// TODO:Implement the BitReader::ReadFloatBits which this function call
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int	ParserUtils::ReadMatrix(const char *data)
{
	int readed = 0;
	unsigned offset = 0;
	
	bool hasScale		= false;	
	readed = BitReader::ReadBoolBit( hasScale, data, offset);
	assert( 1==readed);
	offset += readed;

	// SCALE : optional
	long result = 0;
	if( hasScale )
	{		
		long NScaleBits = 0;
		int readed = BitReader::ReadIntBits( NScaleBits, data, offset, 5);
		offset += 5;
		assert( 5 == readed );

		assert( 16==NScaleBits || 32==NScaleBits);
		
		float scaleX = 0.0f;
		float scaleY = 0.0f;
		readed = BitReader::ReadFixedBits( scaleX, data, offset, NScaleBits);
		assert( readed == NScaleBits );
		offset += NScaleBits;

		readed = BitReader::ReadFixedBits( scaleY, data, offset, NScaleBits);
		assert( readed == NScaleBits );
		offset += NScaleBits;
	}

	// ROTATION : optional
	bool hasRotation	= false;
	readed = BitReader::ReadBoolBit(hasRotation, data, offset);
	assert( 1==readed);
	offset += readed;

	if( hasRotation )
	{
		long NRotateBits = 0;
		int readed = BitReader::ReadIntBits( NRotateBits, data, offset, 5);
		offset += 5;
		assert( 5 == readed );

		assert( 16==NRotateBits || 32==NRotateBits );

		float rotateX = 0.0f;
		float rotateY = 0.0f;
		readed = BitReader::ReadFixedBits( rotateX, data, offset, NRotateBits);
		assert( readed == NRotateBits );
		offset += NRotateBits;

		readed = BitReader::ReadFixedBits( rotateY, data, offset, NRotateBits);
		assert( readed == NRotateBits );
		offset += NRotateBits;		
	}

	// TRANSLATION : mandatory

	long NTranslateBits = 0;	
	readed = BitReader::ReadIntBits( NTranslateBits, data, offset, 5);
	assert( 5 == readed );
	offset += 5;	
	
	long t0 = 0;
	long t1 = 0;
	BitReader::ReadIntBits( t0, data, offset, NTranslateBits);
	offset += NTranslateBits;

	BitReader::ReadIntBits( t1, data, offset, NTranslateBits);
	offset += NTranslateBits;
	
	// translation unit is in twips
	long TranslateX = t0;
	long TranslateY = t1;

	float fx = (float) t0 / 20.0f;
	float fy = (float) t1 / 20.0f;


	printf("tx ty : %d %d\n", t0, t1);
	return (int)ceil( (float)(offset / 8) );
}