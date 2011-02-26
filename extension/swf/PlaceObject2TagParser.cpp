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
	UI8 flags = ReadData<UI8>(data, 0);
	assert( 1 == sizeof( UI8 ) );
	long localOffset = sizeof(UI8);

	UI16 depth = ReadData<UI16>(data, localOffset);
	localOffset += sizeof(UI16);

	bool PlaceFlagHasClipActions	= ReadBit( flags, 7 );
	bool PlaceFlagHasClipDepth		= ReadBit( flags, 6 );
	bool PlaceFlagHasName			= ReadBit( flags, 5 );
	bool PlaceFlagHasRatio			= ReadBit( flags, 4 );
	bool PlaceFlagHasColorTransform = ReadBit( flags, 3 );
	bool PlaceFlagHasMatrix			= ReadBit( flags, 2 );
	bool PlaceFlagHasCharacter		= ReadBit( flags, 1 );
	bool PlaceFlagMove				= ReadBit( flags, 0 );

	bool read = true;
	if( PlaceFlagHasCharacter )
	{
		read = false;
	}
	if( PlaceFlagHasMatrix )
	{
		int byteReaded = this->ReadMatrix( data );
		localOffset += byteReaded;


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
		const char * name = (const char *)(data + localOffset);
		printf("name=%s\n", name);
	}
	return true;
}

//------------------------------------------------------------------------------
int	PlaceObject2TagParser::ReadMatrix(const char *data)
{
	UI8 firstByte = ReadData<UI8>( data, 0 );
	bool hasScale = ReadBit( firstByte, 7 );
	if( hasScale )
	{


	}

	return 0;
}
//------------------------------------------------------------------------------

