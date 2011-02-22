#include "pch.h"
#include "canvasgradient.h"
#include "color.h"
#include "../render/vg/openvg.h"

using namespace Render;

namespace Dom {

JSClass CanvasGradient::jsClass = {
	"CanvasGradient", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub, JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSBool addColorStop(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	if(!JS_InstanceOf(cx, obj, &CanvasGradient::jsClass, argv)) return JS_FALSE;
	CanvasGradient* self = reinterpret_cast<CanvasGradient*>(JS_GetPrivate(cx, obj));
	if(!self) return JS_FALSE;

	double t;
	JS_ValueToNumber(cx, argv[0], &t);

	JSString* jss = JS_ValueToString(cx, argv[1]);
	char* str = JS_GetStringBytes(jss);

	self->addColorStop((float)t, str);

	return JS_TRUE;
}

static JSFunctionSpec methods[] = {
	{"addColorStop", addColorStop, 2,0,0},
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
	jsObject = JS_NewObject(cx, &jsClass, NULL, NULL);
	VERIFY(JS_SetPrivate(cx, jsObject, this));
	VERIFY(JS_DefineFunctions(cx, jsObject, methods));
	addReference();
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
