#ifndef __render_roCanvas_h__
#define __render_roCanvas_h__

#include "roRenderDriver.h"

namespace ro {

/// HTML 5 compitable canvas
struct Canvas
{
	Canvas();
	~Canvas();

// Operations
	void init(roRDriverContext* context);
	void destroy();

// Draw image
	void drawImage(
		roRDriverTexture* texture,
		float dstx, float dsty
	);

	void drawImage(
		roRDriverTexture* texture,
		float dstx, float dsty, float dstw, float dsth
	);

	void drawImage(
		roRDriverTexture* texture,
		float srcx, float srcy, float srcw, float srch,
		float dstx, float dsty, float dstw, float dsth
	);

// Private
	roRDriverContext* _context;
	roRDriverShader* _vShader;
	roRDriverShader* _pShader;
};	// Canvas

}	// namespace ro

#endif	// __render_roCanvas_h__
