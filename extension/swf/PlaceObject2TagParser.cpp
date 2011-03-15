#include "pch.h"
#include "PlaceObject2TagParser.h"
#include "SWFDefines.h"
#include "SWFParserUtils.h"

#include <stdio.h>
#include <cassert>

//------------------------------------------------------------------------------
TSwfTagType	PlaceObject2TagParser::GetTagType() const
{
	return PlaceObject2;
}

//------------------------------------------------------------------------------
bool PlaceObject2TagParser::Parse(const char * data, long length)
{
	long byteOffset = 0;
	
	bool PlaceFlagHasClipActions	= false;
	bool PlaceFlagHasClipDepth		= false;
	bool PlaceFlagHasName			= false;
	bool PlaceFlagHasRatio			= false;
	bool PlaceFlagHasColorTransform = false;
	bool PlaceFlagHasMatrix			= false;
	bool PlaceFlagHasCharacter		= false;
	bool PlaceFlagMove				= false;

	char firstByte = *data;

	BitReader::ReadBoolBit( PlaceFlagHasClipActions, data, 0);
	BitReader::ReadBoolBit( PlaceFlagHasClipDepth, data, 1);
	BitReader::ReadBoolBit( PlaceFlagHasName, data, 2);
	BitReader::ReadBoolBit( PlaceFlagHasRatio, data, 3);
	BitReader::ReadBoolBit( PlaceFlagHasColorTransform, data, 4);
	BitReader::ReadBoolBit( PlaceFlagHasMatrix, data, 5);
	BitReader::ReadBoolBit( PlaceFlagHasCharacter, data, 6);
	BitReader::ReadBoolBit( PlaceFlagMove, data, 7);
	
	byteOffset = 1;

	UI16 depth = ReadData<UI16>(data, 1);
	byteOffset += sizeof(UI16);
				
	bool read = true;
	if( PlaceFlagHasCharacter )
	{
		UI16 CharacterId = ReadData<UI16>( data, byteOffset);
		byteOffset += sizeof(UI16);		
	}
	if( PlaceFlagHasMatrix )
	{
		int byteReaded = ReadMatrix( data + byteOffset );
		byteOffset += byteReaded;
		read = false;
	}
	if( PlaceFlagHasColorTransform )
	{
		read = false;
	}
	if( PlaceFlagHasRatio )
	{
		read = false;
	}
	if( read && PlaceFlagHasName )
	{
		const char * name = (const char *)(data + byteOffset);
		printf("name=%s\n", name);
	}
	return true;
}

// MATRIX RECORD
// reference: http://www.adobe.com/content/dam/Adobe/en/devnet/swf/pdf/swf_file_format_spec_v10.pdf 
// p.20
//------------------------------------------------------------------------------
// TODO:Implement the BitReader::ReadFloatBits which this function call
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int	PlaceObject2TagParser::ReadMatrix(const char *data)
{
	return ParserUtils::ReadMatrix(data);	
}
//------------------------------------------------------------------------------