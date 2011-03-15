#ifndef __SWFTagParser__h__
#define __SWFTagParser__h__

#include "SWFDefines.h"

class SWFTagParser
{
public:
	virtual ~SWFTagParser() {};
	virtual TSwfTagType	GetTagType() const = 0;
	virtual bool Parse(const char * data, long length) = 0;
};

#endif