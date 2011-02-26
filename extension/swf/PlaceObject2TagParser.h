#ifndef __PlaceObject2TagParser__h__
#define __PlaceObject2TagParser__h__

#include "SWFTagParser.h"

//------------------------------------------------------------------------------
class PlaceObject2TagParser : public SWFTagParser
{
public:
	virtual TSwfTagType	GetTagType() const;
	virtual bool Parse(const char * data, long length);

protected:
	// return the bytes readed
	int	ReadMatrix(const char *data);
};

//------------------------------------------------------------------------------

#endif