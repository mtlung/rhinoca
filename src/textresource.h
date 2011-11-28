#ifndef __TEXTRESOURCE_H__
#define __TEXTRESOURCE_H__

#include "resource.h"

/// A text based resource
class TextResource : public Resource
{
public:
	explicit TextResource(const char* uri);

// Operations

// Attributes
	ro::String data;

	const char* dataWithoutBOM;
};	// TextResource

typedef IntrusivePtr<TextResource> TextResourcePtr;

#endif	// __TEXTRESOURCE_H__
