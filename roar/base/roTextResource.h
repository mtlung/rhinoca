#ifndef __roTextResource_h__
#define __roTextResource_h__

#include "roResource.h"

namespace ro {

/// A text based resource
struct TextResource : public ro::Resource
{
	explicit TextResource(const char* uri);

// Attributes
	ro::String data;

	const char* dataWithoutBOM;
};	// TextResource

typedef ro::SharedPtr<TextResource> TextResourcePtr;

}	// namespace ro

#endif	// __roTextResource_h__
