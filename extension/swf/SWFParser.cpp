#include "pch.h"
#include "SWFParser.h"
#include "SWFDefines.h"
#include "SWFParserUtils.h"

#include "DefineShapeTagParser.h"
#include "PlaceObject2TagParser.h"
#include "SetBackgroundColorTagParser.h"

#include <cassert>
#include <stdio.h>

//------------------------------------------------------------------------------
bool SWFParser::Parse( const char * filename )
{
	printf("Loading %s\n", filename);
	FILE * fp = fopen( filename , "rb" );
	assert( fp );

	SWFHeader header;
	fread( & header, sizeof(SWFHeader), 1, fp );

	// this swf is NOT Compressed
	if( 'S' == header.signature[2] )
	{
	}
	else if( 'C' == header.signature[2] ) // this swf is Compressed
	{
		assert( !"Error:Compressed swf NOT suppported yet" );
	}

	// read the whole file in 1 block
	long size = header.fileLength - sizeof( header );
	char * buffer = new char[ size ];
	RECT rect;
	fread( buffer, sizeof(char), size, fp );
	fclose( fp );

	long byteReaded = ReadRECT( & rect, buffer );

	unsigned short frameRateFixed8Dot8  = 0;
	unsigned short frameCount = 0;

	frameRateFixed8Dot8 = ReadData<UI16>( buffer, byteReaded );
	byteReaded += sizeof( unsigned short);

	frameCount = ReadData<UI16>( buffer, byteReaded );
	byteReaded += sizeof( unsigned short);

	printf("frameCount = %d\n", frameCount);

	if( header.version >= 8 )
	{
		// require FileAttributes tag which not supported at the moment
		assert( ! "Die now!" );
	}

	int showFrameCount = 0;

	for( ;; )
	{
		unsigned short type	= 0;
		unsigned int length = 0;
		int readed = ReadTagCodeAndLength( buffer, byteReaded, & type, & length );
		byteReaded += readed;

		if( length >= MaxShortTagLength )
		{
			SI32 longLength = 0;
			longLength = ReadData<SI32>(buffer, byteReaded);
			byteReaded += sizeof(SI32);
			length = longLength;
		}

		const char * tagTypeName = TagTypeToString( type );
		assert( NULL != tagTypeName );
		printf("%s:%d\n", tagTypeName, length);

		if( TSwfTagType::End == type )
		{
			break;
		}
		if( TSwfTagType::ShowFrame == type )
		{
			++ showFrameCount;
		}
		if( TSwfTagType::DefineShape == type )
		{
			const char * data = buffer + byteReaded;
			DefineShapeTagParser p;
			p.Parse( data, length );
		}
		if( TSwfTagType::PlaceObject2 == type )
		{
			const char * data = buffer + byteReaded;
			PlaceObject2TagParser p ;
			p.Parse( data, length);
		}
		if( TSwfTagType::SetBackgroundColor == type )
		{
			const char * data = buffer + byteReaded;
			SetBackgroundColorTagParser p;
			p.Parse(data, length);
		}

		float percent = 100.0f * byteReaded / header.fileLength;

		printf("%f loaded\n", percent);

		byteReaded += length;
	}
	printf("Done\n");

	if( buffer )
	{
		delete [] buffer;
	}
	printf("%d ShowFrame Found!\n", showFrameCount);
	return true;
}


//------------------------------------------------------------------------------