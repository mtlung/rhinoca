#ifndef __TEXTRESOURCE_H__
#define __TEXTRESOURCE_H__

#include "../roar/base/roResource.h"

/// A text based resource
class TextResource : public ro::Resource
{
public:
	explicit TextResource(const char* uri);

// Operations

// Attributes
	ro::String data;

	const char* dataWithoutBOM;
};	// TextResource

typedef ro::SharedPtr<TextResource> TextResourcePtr;

#endif	// __TEXTRESOURCE_H__
