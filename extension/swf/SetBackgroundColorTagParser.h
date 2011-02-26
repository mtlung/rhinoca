#ifndef __SetBackgroundColorTagParser__h__
#define __SetBackgroundColorTagParser__h__

#include "SWFTagParser.h"

//------------------------------------------------------------------------------
class SetBackgroundColorTagParser : public SWFTagParser
{
public:
	virtual TSwfTagType	GetTagType() const;
	virtual bool Parse(const char * data, long length);
};

//------------------------------------------------------------------------------
#endif