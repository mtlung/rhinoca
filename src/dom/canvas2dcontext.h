#ifndef __DOM_CANVAS2DCONTEXT_H__
#define __DOM_CANVAS2DCONTEXT_H__

#include "canvas.h"
#include "../mat44.h"
#include "../vector.h"

namespace Dom {

class CanvasGradient;

class CanvasRenderingContext2D : public HTMLCanvasElement::Context
{
public:
	explicit CanvasRenderingContext2D(HTMLCanvasElement*);
	~CanvasRenderingContext2D();

// Operations
	override void bind(JSContext* cx, JSObject* parent);

	void beginLayer();
	void endLayer();

	void clearRect(float x, float y, float w, float h);

// States
	/// Pushes the current state onto the stack.
	void save();

	/// Pops the top state on the stack, restoring the context to that state.
	void restore();

// Draw image
	void drawImage(
		Render::Texture* texture,
		float dstx, float dsty
	);

	void drawImage(
		Render::Texture* texture,
		float dstx, float dsty, float dstw, float dsth
	);

	void drawImage(
		Render::Texture* texture,
		float srcx, float srcy, float srcw, float srch,
		float dstx, float dsty, float dstw, float dsth
	);

// Transforms
	/// Changes the transformation matrix to apply a scaling transformation with the given characteristics.
	void scale(float x, float y);

	/// Changes the transformation matrix to apply a rotation transformation with the given characteristics.
	/// The angle is in radians.
	void rotate(float angle);

	/// Changes the transformation matrix to apply a translation transformation with the given characteristics.
	void translate(float x, float y);

	/// Changes the transformation matrix to apply the matrix given by the arguments as described below.
	void transform(float m11, float m12, float m21, float m22, float dx, float dy);

	/// Changes the transformation matrix to the matrix given by the arguments as described below.
	void setTransform(float m11, float m12, float m21, float m22, float dx, float dy);

// Paths
	/// Resets the current path.
	void beginPath();

	/// Marks the current subpath as closed, and starts a new subpath with
	/// a point the same as the start and end of the newly closed subpath.
	void closePath();

	/// Creates a new subpath with the given point.
	void moveTo(float x, float y);

	/// Adds the given point to the current subpath, connected to the
	/// previous one by a straight line.
	void lineTo(float x, float y);

	/// Adds the given point to the current path, connected to the previous
	/// one by a quadratic Bezier curve with the given control point.
	void quadrativeCureTo(float cpx, float cpy, float x, float y);

	/// Adds the given point to the current path, connected to the previous
	/// one by a cubic Bezier curve with the given control points.
	void bezierCurveTo(float cp1x, float cp1y, float cp2x, float cp2y, float x, float y);

	/// Adds a point to the current path, connected to the previous one b
	/// a straight line, then adds a second point to the current path, connected
	/// to the previous one by an arc whose properties are described by the arguments.
	void arcTo(float x1, float y1, float x2, float y2, float radius);

	/// Adds points to the subpath such that the arc described by the
	/// circumference of the circle described by the arguments, starting at
	/// the given start angle and ending at the given end angle, going in
	/// the given direction, is added to the path, connected to the previous
	/// point by a straight line.
	void arc(float x, float y, float radius, float startAngle, float endAngle, bool anticlockwise);

	/// Adds a new closed subpath to the path, representing the given rectangle.
	void rect(float x, float y, float w, float h);

	/// Returns true if the given point is in the current path.
	void isPointInPath(float x, float y);

// Stroke
	/// Strokes the subpaths with the current stroke style.
	void stroke();

	void strokeRect(float x, float y, float w, float h);

	void setStrokeColor(float* rgba);

	void setStrokeGradient(CanvasGradient* gradient);

	/// Calue values are 'butt', 'round' and 'square'
	void setLineCap(const char* cap);

	/// Calue values are 'round', 'bevel' and 'miter'
	void setLineJoin(const char* join);

	void setLineWidth(float width);

// Fill
	/// Fills the subpaths with the current fill style.
	void fill();

	void fillRect(float x, float y, float w, float h);

	void setFillColor(float* rgba);

	void setFillGradient(CanvasGradient* gradient);

	/// Further constrains the clipping region to the given path.
	void clip();

// Attributes
	unsigned width() const { return canvas->width(); }
	unsigned height() const { return canvas->height(); }

	float globalAlpha() const { return _globalAlpha; }
	void setGlobalAlpha(float alpha);

	struct OpenVG;
	OpenVG* openvg;

	struct State {
		Mat44 transform;
	};
	State currentState;
	Vector<State> stateStack;

	float _globalAlpha;

	static JSClass jsClass;
};	// CanvasRenderingContext2D

}	// namespace Dom

#endif	// __DOM_CANVAS2DCONTEXT_H__
