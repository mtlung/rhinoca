#include "pch.h"
#include "DefineShapeTagParser.h"
#include "SWFDefines.h"

//------------------------------------------------------------------------------
TSwfTagType	DefineShapeTagParser::GetTagType() const
{
	return DefineShape;
}

//------------------------------------------------------------------------------
bool DefineShapeTagParser::Parse(const char * data, long length)
{
	return true;
}
//------------------------------------------------------------------------------