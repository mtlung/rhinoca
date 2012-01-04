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
	void init				(roRDriverContext* context);
	bool initTargetTexture	(unsigned width, unsigned height);
	void destroy			();

	void beginDraw			();
	void endDraw			();

// States
	void save				();	/// Pushes the current state onto the stack.
	void restore			();	/// Pops the top state on the stack, restoring the context to that state.

// Draw image
	void drawImage			(roRDriverTexture* texture, float dstx, float dsty);
	void drawImage			(roRDriverTexture* texture, float dstx, float dsty, float dstw, float dsth);
	void drawImage			(roRDriverTexture* texture, float srcx, float srcy, float srcw, float srch, float dstx, float dsty, float dstw, float dsth);

// Transforms
	void scale				(float x, float y);
	void rotate				(float angle);
	void translate			(float x, float y);
	void transform			(float m11, float m12, float m21, float m22, float dx, float dy);
	void transform			(float mat44[16]);
	void setTransform		(float m11, float m12, float m21, float m22, float dx, float dy);
	void setTransform		(float mat44[16]);
	void setIdentity		();

// Attributes
	unsigned width() const;
	unsigned height() const;

	float globalAlpha() const;
	void setGlobalAlpha(float alpha);

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

	float _globalAlpha;

};	// Canvas

}	// namespace ro

#endif	// __render_roCanvas_h__
