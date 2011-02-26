#ifndef __DefineShapeTagParser__h__
#define __DefineShapeTagParser__h__

#include "SWFTagParser.h"

//------------------------------------------------------------------------------
class DefineShapeTagParser : public SWFTagParser
{
public:
	TSwfTagType	GetTagType() const;
	bool Parse(const char * data, long length);
};

//------------------------------------------------------------------------------
#endif