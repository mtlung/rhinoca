#include "pch.h"
#include "SetBackgroundColorTagParser.h"
#include "SWFDefines.h"
#include "SWFParserUtils.h"

#include <stdio.h>

#include <cassert>

//------------------------------------------------------------------------------
TSwfTagType	SetBackgroundColorTagParser::GetTagType() const
{
	return SetBackgroundColor;
}

//------------------------------------------------------------------------------
bool SetBackgroundColorTagParser::Parse(const char * data, long length)
{
	UI8 r = ReadData<UI8>( data, 0 );
	UI8 g = ReadData<UI8>( data, 1 );
	UI8 b = ReadData<UI8>( data, 2 );
	printf( "rgb=(%d %d %d)\n", r, g, b );
	return true;
}

//------------------------------------------------------------------------------
