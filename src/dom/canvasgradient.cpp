#include "pch.h"
#include "canvasgradient.h"
#include "color.h"
#include "../render/vg/openvg.h"

using namespace Render;

namespace Dom {

JSClass CanvasGradient::jsClass = {
	"CanvasGradient", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub, JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSBool addColorStop(JSContext* cx, uintN argc, jsval* vp)
{
	CanvasGradient* self = getJsBindable<CanvasGradient>(cx, vp);
	if(!self) return JS_FALSE;

	double t;
	VERIFY(JS_ValueToNumber(cx, JS_ARGV0, &t));

	JsString jss(cx, JS_ARGV1);
	self->addColorStop((float)t, jss.c_str());

	return JS_TRUE;
}

static JSFunctionSpec methods[] = {
	{"addColorStop", addColorStop, 2,0},
	{0}
};

CanvasGradient::CanvasGradient()
	: handle(NULL)
	, stops(NULL)
	, stopCount(0)
{
}

CanvasGradient::~CanvasGradient()
{
	vgDestroyPaint(handle);
	rhdelete(stops);
}

void CanvasGradient::bind(JSContext* cx, JSObject* parent)
{
	ASSERT(!jsContext);
	jsContext = cx;
	jsObject = JS_NewObject(cx, &jsClass, NULL, parent);
	VERIFY(JS_SetPrivate(cx, *this, this));
	VERIFY(JS_DefineFunctions(cx, *this, methods));
	addReference();	// releaseReference() in JsBindable::finalize()
}

void CanvasGradient::createLinear(float xStart, float yStart, float xEnd, float yEnd)
{
	ASSERT(!handle);
	handle = vgCreatePaint();
	vgSetParameteri(handle, VG_PAINT_COLOR_RAMP_SPREAD_MODE, VG_COLOR_RAMP_SPREAD_PAD);
	vgSetParameteri(handle, VG_PAINT_TYPE, VG_PAINT_TYPE_LINEAR_GRADIENT);
	const float linear[] = { xStart, yStart ,xEnd, yEnd };
	vgSetParameterfv(handle, VG_PAINT_LINEAR_GRADIENT, 4, linear);
}

void CanvasGradient::createRadial(float xStart, float yStart, float radiusStart, float xEnd, float yEnd, float radiusEnd)
{
	ASSERT(!handle);
	handle = vgCreatePaint();
	vgSetParameteri(handle, VG_PAINT_COLOR_RAMP_SPREAD_MODE, VG_COLOR_RAMP_SPREAD_PAD);
	vgSetParameteri(handle, VG_PAINT_TYPE, VG_PAINT_TYPE_RADIAL_GRADIENT);
	_radiusStart = radiusStart;
	_radiusEnd = radiusEnd;
	const float radial[] = { xEnd, yEnd, xStart, yStart ,radiusEnd };
	vgSetParameterfv(handle, VG_PAINT_RADIAL_GRADIENT, 5, radial);
}

struct ColorStop {
	float t, r, g, b, a;
};

void CanvasGradient::addColorStop(float offset, const char* color)
{
	Color c;
	c.parse(color);

	// We need to adjust the offset for radial fill, to deal with starting radius
	if(vgGetParameteri(handle, VG_PAINT_TYPE) == VG_PAINT_TYPE_RADIAL_GRADIENT)
	{
		// Add an extra stop as the starting radius
		if(stops == 0) {
			stops = (float*)rhnew<ColorStop>(1);
			ColorStop* s = reinterpret_cast<ColorStop*>(stops);

			s[stopCount].t = _radiusStart / _radiusEnd;
			s[stopCount].r = c.r;
			s[stopCount].g = c.g;
			s[stopCount].b = c.b;
			s[stopCount].a = c.a;
			++stopCount;
		}

		offset *= 1.0f - _radiusStart / _radiusEnd;
		offset += _radiusStart / _radiusEnd;
	}

	stops = (float*)rhrenew<ColorStop>((ColorStop*)stops, stopCount, stopCount + 1);
	ColorStop* s = reinterpret_cast<ColorStop*>(stops);

	s[stopCount].t = offset;
	s[stopCount].r = c.r;
	s[stopCount].g = c.g;
	s[stopCount].b = c.b;
	s[stopCount].a = c.a;
	++stopCount;
}

}	// namespace Dom
