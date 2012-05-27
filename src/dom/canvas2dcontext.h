#ifndef __DOM_CANVAS2DCONTEXT_H__
#define __DOM_CANVAS2DCONTEXT_H__

#include "canvas.h"
//#include "../../roar/base/roArray.h"
//#include "../../roar/math/roMatrix.h"
#include "../../roar/render/roCanvas.h"

namespace Dom {

class CanvasGradient;
class ImageData;

class CanvasRenderingContext2D : public HTMLCanvasElement::Context
{
public:
	explicit CanvasRenderingContext2D(HTMLCanvasElement*);
	~CanvasRenderingContext2D();

// Operations
	override void bind(JSContext* cx, JSObject* parent);

	static void registerClass(JSContext* cx, JSObject* parent);

// Attributes
	override unsigned width() const { return _canvas.width(); }
	override unsigned height() const { return _canvas.width(); }
	override void setWidth(unsigned width);
	override void setHeight(unsigned height);

	static JSClass jsClass;

// Private
	ro::Canvas _canvas;
};	// CanvasRenderingContext2D

}	// namespace Dom

#endif	// __DOM_CANVAS2DCONTEXT_H__
