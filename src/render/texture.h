#ifndef __RENDER_TEXTURE_H__
#define __RENDER_TEXTURE_H__

#include "../common.h"
#include "../resource.h"

namespace Render {

class Texture : public Resource
{
public:
	explicit Texture(const char* uri);
	virtual ~Texture();

	enum Format { RGB, BGR, RGBA, BGRA };

// Operations
	bool create(rhuint width, rhuint height, const char* data, rhuint dataSize, Format dataFormat);

	void clear();

// Attibutes
	rhuint handle;
	rhuint width, height;

protected:
	rhuint _type;
};	// Texture

}	// namespace Render

#endif	// __RENDER_TEXTURE_H__
