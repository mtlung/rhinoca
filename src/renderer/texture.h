#ifndef __RENDER_TEXTURE_H__
#define __RENDER_TEXTURE_H__

#include "../common.h"
#include "../rhstring.h"

namespace Render {

class Texture : private Noncopyable
{
public:
	Texture();
	~Texture();

	enum Format { RGB, BGR, RGBA, BGRA };

// Operations
	bool create(rhuint width, rhuint height, const char* data, rhuint dataSize, Format dataFormat);

	void clear();

// Attibutes
	rhuint handle;
	FixString url;

protected:
	rhuint _type;
	rhuint _width, _height;
};	// Texture

}	// namespace Render

#endif	// __RENDER_TEXTURE_H__
