#ifndef __DOM_CANVASGRADIENT_H__
#define __DOM_CANVASGRADIENT_H__

#include "canvas.h"

namespace Dom {

class CanvasGradient : public JsBindable
{
public:
	explicit CanvasGradient();
	~CanvasGradient();

// Operations
	override void bind(JSContext* cx, JSObject* parent);

	void createLinear(float xStart, float yStart, float xEnd, float yEnd);

	void createRadial(float xStart, float yStart, float radiusStart, float xEnd, float yEnd, float radiusEnd);

	void addColorStop(float offset, const char* color);

// Attribbutes
	enum Type {
		Linear = 0x1A04,	// Values come from openvg.h
		Radial = 0x1A05,
	};

	void* handle;
	float* stops;
	unsigned stopCount;
	float _radiusStart, _radiusEnd;	// For radial fill

	static JSClass jsClass;
};	// CanvasGradient

}	// namespace Dom

#endif	// __DOM_CANVASGRADIENT_H__
