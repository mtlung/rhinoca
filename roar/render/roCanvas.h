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
	void init					();
	bool initTargetTexture		(unsigned width, unsigned height);
	void destroy				();

	void beginDraw				();
	void endDraw				();

	void clearRect				(float x, float y, float w, float h);	/// Clear the rect to transparent black

// States
	void save					();	/// Pushes the current state onto the stack.
	void restore				();	/// Pops the top state on the stack, restoring the context to that state.

// Draw image
	void drawImage				(roRDriverTexture* texture, float dstx, float dsty);
	void drawImage				(roRDriverTexture* texture, float dstx, float dsty, float dstw, float dsth);
	void drawImage				(roRDriverTexture* texture, float srcx, float srcy, float srcw, float srch, float dstx, float dsty, float dstw, float dsth);

// Batching
	void beginDrawImageBatch	();	/// For best performance, sort the call to drawImage() by the texture used
	void endDrawImageBatch		();

// Pixel manipulation
	float* lockPixelData		();
	void unlockPixelData		();

// Transforms
	void scale					(float x, float y);
	void rotate					(float clockwiseRadian);
	void translate				(float x, float y);
	void transform				(float m11, float m12, float m21, float m22, float dx, float dy);
	void transform				(float mat44[16]);
	void setTransform			(float m11, float m12, float m21, float m22, float dx, float dy);
	void setTransform			(float mat44[16]);
	void setIdentity			();

// Paths
	void beginPath				();					/// Resets the current path.
	void closePath				();					/// Marks the current subpath as closed, and starts a new subpath with a point the same as the start and end of the newly closed subpath.
	void moveTo					(float x, float y);	/// Creates a new subpath with the given point.
	void lineTo					(float x, float y);	/// Adds the given point to the current subpath, connected to the previous one by a straight line.
	void quadraticCurveTo		(float cpx, float cpy, float x, float y);
	void bezierCurveTo			(float cp1x, float cp1y, float cp2x, float cp2y, float x, float y);
	void arcTo					(float x1, float y1, float x2, float y2, float radius);
	void arc					(float x, float y, float radius, float startAngle, float endAngle, bool anticlockwise);
	void rect					(float x, float y, float w, float h);
	bool isPointInPath			(float x, float y);

// Gradient
	void* createLinearGradient	(float xStart, float yStart, float xEnd, float yEnd);
	void* createRadialGradient	(float xStart, float yStart, float radiusStart, float xEnd, float yEnd, float radiusEnd);
	void addGradientColorStop	(void* gradient, float offset, float r, float g, float b, float a);
	void destroyGradient		(void* gradient);

// Stroke
	void stroke					();					/// Strokes the subpaths with the current stroke style.
	void strokeRect				(float x, float y, float w, float h);
	void getStrokeColor			(float* rgba);
	void setStrokeColor			(const float* rgba);
	void setStrokeColor			(float r, float g, float b, float a);
	void setLineCap				(const char* cap);	/// Valid values are 'butt', 'round' and 'square'
	void setLineJoin			(const char* join);	/// Valid values are 'round', 'bevel' and 'miter'
	void setLineWidth			(float width);
	void setStrokeGradient		(void* gradient);

// Fill
	void fill					();					/// Fills the subpaths with the current fill style.
	void fillRect				(float x, float y, float w, float h);
	void fillText				(const char* utf8Str, float x, float y, float maxWidth);
	void getFillColor			(float* rgba);
	void setFillColor			(const float* rgba);
	void setFillColor			(float r, float g, float b, float a);
	void setFillGradient		(void* gradient);

	void clip();									/// Further constrains the clipping region to the given path.

// Attributes
	unsigned width				() const;
	unsigned height				() const;

	float globalAlpha			() const;
	void setGlobalAlpha			(float alpha);

	void getglobalColor			(float* rgba);
	void setGlobalColor			(const float* rgba);
	void setGlobalColor			(float r, float g, float b, float a);

	void setFont				(const char* style);	/// "20pt Arial"
	void setTextAlign			(const char* align);	/// Valid values are 'left', 'right' , 'center', 'start' and 'end'
	void setTextBaseline		(const char* baseLine);	/// Valid values are 'top', 'hanging' , 'middle', 'alphabetic', 'ideographic' and 'bottom'

	/// sets how shapes and images are drawn onto the existing bitmap
	/// See: http://www.whatwg.org/specs/web-apps/current-work/multipage/the-canvas-element.html#compositing
	/// Possible values:
	/// source-atop, source-in, source-out, source-over (default), destination-atop,
	/// destination-in, destination-out, destination-over, lighter, copy, xor
	void setComposition			(const char* operation);

	TexturePtr targetTexture;
	TexturePtr depthStencilTexture;

// Private
	void _flushDrawImageBatch();

	roRDriver* _driver;
	roRDriverContext* _context;
	roRDriverBuffer* _vBuffer;
	roRDriverBuffer* _uBuffer;
	roRDriverShader* _vShader;
	roRDriverShader* _pShader;
	roRDriverTextureState _textureState;
	roRDriverShaderTextureInput _textureInput;
	StaticArray<roRDriverShaderBufferInput, 4> _bufferInputs;

	float _targetWidth;
	float _targetHeight;

	bool _isBatchMode;
	unsigned _batchedQuadCount;
	roRDriverTexture* _batchModeCurrentTexture;
	char* _mappedVBuffer;

	struct OpenVG;
	OpenVG* _openvg;

	struct State {
		int lineCap;
		int lineJoin;
		float lineWidth;
		float globalColor[4];
		float strokeColor[4];
		float fillColor[4];
		int compisitionOperation;
		ConstString fontName;
		ro::Mat4 transform;
	};
	State _currentState;
	Mat4 _orthoMat;
	ro::Array<State> _stateStack;
};	// Canvas

}	// namespace ro

#endif	// __render_roCanvas_h__
