#ifndef __render_roCanvas_h__
#define __render_roCanvas_h__

#include "roRenderDriver.h"
#include "roTexture.h"
#include "../base/roArray.h"

namespace ro {

/// HTML 5 compatible canvas
struct Canvas
{
	Canvas();
	~Canvas();

// Operations
	void init(roRDriverContext* context);
	bool initTargetTexture(unsigned width, unsigned height);
	void destroy();

	void beginDraw();
	void endDraw();

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

	TexturePtr targetTexture;

// Private
	roRDriver* _driver;
	roRDriverContext* _context;
	roRDriverBuffer* _vBuffer;
	roRDriverBuffer* _iBuffer;
	roRDriverBuffer* _uBuffer;
	roRDriverShader* _vShader;
	roRDriverShader* _pShader;
	roRDriverTextureState _textureState;
	StaticArray<roRDriverShaderInput, 4> _inputLayout;

	float _targetWidth;
	float _targetHeight;
};	// Canvas

}	// namespace ro

#endif	// __render_roCanvas_h__
