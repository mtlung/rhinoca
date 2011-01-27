#ifndef __DOM_CANVAS2DCONTEXT_H__
#define __DOM_CANVAS2DCONTEXT_H__

#include "canvas.h"
#include "../mat44.h"
#include "../vector.h"

namespace Dom {

class Canvas2dContext : public Canvas::Context
{
public:
	explicit Canvas2dContext(Canvas*);
	~Canvas2dContext();

// Operations
	void bind(JSContext* cx, JSObject* parent);

	void beginLayer();
	void endLayer();

	void clearRect(float x, float y, float w, float h);

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

	void save();
	void restore();
	void scale(float x, float y);
	void rotate(float angle);	///< The angle is radian
	void translate(float x, float y);
	void transform(float m11, float m12, float m22, float dx, float dy);
	void setTransform(float m11, float m12, float m22, float dx, float dy);

// Attributes
	unsigned width() const { return canvas->width(); }
	unsigned height() const { return canvas->height(); }

	struct State {
		Mat44 transform;
	};
	State currentState;
	Vector<State> stateStack;

	static JSClass jsClass;
};	// Canvas2dContext

}	// namespace Dom

#endif	// __DOM_CANVAS2DCONTEXT_H__
