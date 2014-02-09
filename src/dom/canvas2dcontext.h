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
	void bind(JSContext* cx, JSObject* parent) override;

	static void registerClass(JSContext* cx, JSObject* parent);

// Attributes
	unsigned width() const override { return _canvas.width(); }
	unsigned height() const override { return _canvas.width(); }
	void setWidth(unsigned width) override;
	void setHeight(unsigned height) override;
	ro::Texture* texture();

	static JSClass jsClass;

// Private
	ro::Canvas _canvas;
};	// CanvasRenderingContext2D

}	// namespace Dom

#endif	// __DOM_CANVAS2DCONTEXT_H__
