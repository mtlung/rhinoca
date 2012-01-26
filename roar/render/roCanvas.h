#ifndef __render_roCanvas_h__
#define __render_roCanvas_h__

#include "roRenderDriver.h"
#include "roTexture.h"
#include "../base/roArray.h"
#include "../math/roMatrix.h"

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
	void rotate				(float clockwiseRadian);
	void translate			(float x, float y);
	void transform			(float m11, float m12, float m21, float m22, float dx, float dy);
	void transform			(float mat44[16]);
	void setTransform		(float m11, float m12, float m21, float m22, float dx, float dy);
	void setTransform		(float mat44[16]);
	void setIdentity		();

// Paths
	void beginPath			();					/// Resets the current path.
	void closePath			();					/// Marks the current subpath as closed, and starts a new subpath with a point the same as the start and end of the newly closed subpath.
	void moveTo				(float x, float y);	/// Creates a new subpath with the given point.
	void lineTo				(float x, float y);	/// Adds the given point to the current subpath, connected to the previous one by a straight line.
	void quadraticCurveTo	(float cpx, float cpy, float x, float y);
	void bezierCurveTo		(float cp1x, float cp1y, float cp2x, float cp2y, float x, float y);
	void arcTo				(float x1, float y1, float x2, float y2, float radius);
	void arc				(float x, float y, float radius, float startAngle, float endAngle, bool anticlockwise);
	void rect				(float x, float y, float w, float h);
	bool isPointInPath		(float x, float y);

	// Stroke
	void stroke				();					/// Strokes the subpaths with the current stroke style.
	void strokeRect			(float x, float y, float w, float h);
	void getStrokeColor		(float* rgba);
	void setStrokeColor		(const float* rgba);
//	void setStrokeGradient	(CanvasGradient* gradient);
	void setLineCap			(const char* cap);	/// Valid values are 'butt', 'round' and 'square'
	void setLineJoin		(const char* join);	/// Valid values are 'round', 'bevel' and 'miter'
	void setLineWidth		(float width);

// Fill
	void fill				();					/// Fills the subpaths with the current fill style.
	void fillRect			(float x, float y, float w, float h);
	void getFillColor		(float* rgba);
	void setFillColor		(const float* rgba);
//	void setFillGradient	(CanvasGradient* gradient);

	void clip();								/// Further constrains the clipping region to the given path.

// Batching
	void beginBatch			();
	void endBatch			();

// Attributes
	unsigned width			() const;
	unsigned height			() const;

	float globalAlpha		() const;
	void setGlobalAlpha		(float alpha);

	TexturePtr targetTexture;
	TexturePtr depthStencilTexture;

// Private
	roRDriver* _driver;
	roRDriverContext* _context;
	roRDriverBuffer* _vBuffer;
	roRDriverBuffer* _uBuffer;
	roRDriverShader* _vShader;
	roRDriverShader* _pShader;
	roRDriverTextureState _textureState;
	StaticArray<roRDriverShaderInput, 3> _inputLayout;

	float _targetWidth;
	float _targetHeight;

	struct OpenVG;
	OpenVG* _openvg;

	struct State {
		int lineCap;
		int lineJoin;
		float lineWidth;
		float globalAlpha;
		float strokeColor[4];
		float fillColor[4];
		ro::Mat4 transform;
	};
	State _currentState;
	Mat4 _orthoMat;
	ro::Array<State> _stateStack;
};	// Canvas

}	// namespace ro

#endif	// __render_roCanvas_h__
