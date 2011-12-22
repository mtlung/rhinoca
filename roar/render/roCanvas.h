#ifndef __render_roCanvas_h__
#define __render_roCanvas_h__

#include "roRenderDriver.h"
#include "../base/roArray.h"

namespace ro {

/// HTML 5 compatible canvas
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
	roRDriver* _driver;
	roRDriverContext* _context;
	roRDriverBuffer* _vBuffer;
	roRDriverBuffer* _iBuffer;
	roRDriverShader* _vShader;
	roRDriverShader* _pShader;
	roRDriverTextureState _textureState;
	StaticArray<roRDriverShaderInput, 3> _inputLayout;
};	// Canvas

}	// namespace ro

#endif	// __render_roCanvas_h__
