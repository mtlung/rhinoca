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

	void init(JSContext* cx, unsigned w, unsigned h, const unsigned char* rawData=NULL);

// Attribbutes
	unsigned width, height;

	rhbyte* rawData();
	unsigned length() const { return width * height * sizeof(rhbyte); }

	JSObject* array;

	static JSClass jsClass;
};	// ImageData

}	// namespace Dom

#endif	// __DOM_IMAGEDATA_H__
