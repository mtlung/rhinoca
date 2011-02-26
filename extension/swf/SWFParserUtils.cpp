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
	static const char* map[] = {
		"End",
		"ShowFrame",
		"DefineShape",
		NULL,
		"PlaceObject",
		"RemoveObject",
		"DefineBits",
		"DefineButton",
		"JPEGTables",
		"SetBackgroundColor",
		"DefineFont",
		"DefineText",
		"DoAction",
		"DefineFontInfo",
		"DefineSound",
		"StartSound",
		NULL,
		"DefineButtonSound",
		"SoundStreamHead",
		"SoundStreamBlock",
		"DefineBitsLossless",
		"DefineBitsJPEG2",
		"DefineShape2",
		"DefineButtonCxform",
		"Protect",
		NULL,
		"PlaceObject2",
		NULL,
		"RemoveObject2",
		NULL,
		NULL,
		NULL,
		"DefineShape3",
		"DefineText2",
		"DefineButton2",
		"DefineBitsJPEG3",
		"DefineBitsLossless2",
		"DefineEditText",
		NULL,
		"DefineSprite",
		"FrameLabel",
		"SoundStreamHead2",
		"DefineMorphShape",
		"DefineFont2",
		"ExportAssets",
		"ImportAssets",
		"EnableDebugger",
		"DoInitAction",
		"DefineVideoStream",
		"VideoFrame",
		"DefineFontInfo2",
		"EnableDebugger2",
		"ScriptLimits",
		"SetTabIndex",
		"FileAttributes",
		"PlaceObject3",
		"ImportAssets2",
		"DefineFontAlignZones",
		"CSMTextSettings",
		"DefineFont3",
		"SymbolClass",
		"Metadata",
		"DefineScalingGrid",
		"DoABC",
		"DefineShape4",
		"DefineMorphShape2",
		"DefineSceneAndFrameLabelData",
		"DefineBinaryData",
		"DefineFontName",
		"StartSound2",
		"DefineBitsJPEG4",
		"DefineFont4",
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