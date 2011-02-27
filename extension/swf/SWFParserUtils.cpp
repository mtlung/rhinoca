#include "pch.h"
#include "SWFParserUtils.h"

#include <math.h>
#include <cassert>

#ifndef NULL
#	define NULL 0
#endif

//------------------------------------------------------------------------------
bool ReadBit( const UI8 data, unsigned char position )
{
	assert( position <= 7 );
	UI8 maskes[] = { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };
	UI8 mask = maskes[position];	// 1 << position;
	return (data & mask) > 0;
}

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

//------------------------------------------------------------------------------
int	ReadRECT( RECT * rect, char * buffer )
{
	unsigned nBits = (unsigned char) buffer[0];
	unsigned short mask = 255 << (8-5);
	unsigned short bits = nBits & mask;
	bits = bits >> (8-5);
	int nByte = (int)ceil( (float)(4 * bits + 5)/ 8 );

	assert( bits <= 31 );

	rect->xMin = 0;
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