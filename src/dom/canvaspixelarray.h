#ifndef __DOM_CANVASPIXELARRAY_H__
#define __DOM_CANVASPIXELARRAY_H__

#include "../jsbindable.h"

namespace Dom {

/// A one dimension array representing the pixels
class CanvasPixelArray : public JsBindable
{
public:
	CanvasPixelArray();
	~CanvasPixelArray();

// Operations
	override void bind(JSContext* cx, JSObject* parent);

// Attribbutes
	unsigned length;

	unsigned char getAt(unsigned index) const { return rawData[index]; }

	void setAt(unsigned index, unsigned char value) { rawData[index] = value; }

	/// Pointer to RGBA data, order from left to right, then top to bottom
	unsigned char* rawData;

	static JSClass jsClass;
};	// CanvasPixelArray

}	// namespace Dom

#endif	// __DOM_CANVASPIXELARRAY_H__
