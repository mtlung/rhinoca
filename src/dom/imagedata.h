#ifndef __DOM_IMAGEDATA_H__
#define __DOM_IMAGEDATA_H__

#include "canvas.h"

namespace Dom {

class CanvasPixelArray;

class ImageData : public JsBindable
{
public:
	explicit ImageData();
	~ImageData();

// Operations
	override void bind(JSContext* cx, JSObject* parent);

	void init(unsigned w, unsigned h, const unsigned char* rawData=NULL);

// Attribbutes
	unsigned width, height;
	CanvasPixelArray* data;

	static JSClass jsClass;
};	// ImageData

}	// namespace Dom

#endif	// __DOM_IMAGEDATA_H__
