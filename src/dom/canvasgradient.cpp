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
};

CanvasGradient::CanvasGradient()
	: handle(NULL)
{
}

CanvasGradient::~CanvasGradient()
{
	vgDestroyPaint(handle);
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
	vgSetParameteri(handle, VG_PAINT_TYPE, VG_PAINT_TYPE_LINEAR_GRADIENT);
	const float linear[] = { xStart, yStart ,xEnd, yEnd };
	vgSetParameterfv(handle, VG_PAINT_LINEAR_GRADIENT, 4, linear);
}

void CanvasGradient::createRadial(float xStart, float yStart, float radiusStart, float xEnd, float yEnd, float radiusEnd)
{
	ASSERT(!handle);
	handle = vgCreatePaint();
	vgSetParameteri(handle, VG_PAINT_TYPE, VG_PAINT_TYPE_RADIAL_GRADIENT);
}

struct ColorStop {
	float t, r, g, b, a;
};

void CanvasGradient::addColorStop(float offset, const char* color)
{
	Color c;
	c.parse(color);

	VGint size = vgGetParameterVectorSize(handle, VG_PAINT_COLOR_RAMP_STOPS);

	ColorStop* colorStop = rhnew<ColorStop>(size + 1);

	if(size > 0)
		vgGetParameterfv(handle, VG_PAINT_COLOR_RAMP_STOPS, size, (VGfloat*)colorStop);

	colorStop[size*5].t = offset;
	colorStop[size*5].r = c.r;
	colorStop[size*5].g = c.g;
	colorStop[size*5].b = c.b;
	colorStop[size*5].a = c.a;

	vgSetParameterfv(handle, VG_PAINT_COLOR_RAMP_STOPS, (size + 1) * 5, (VGfloat*)colorStop);

	rhdelete(colorStop);
}

}	// namespace Dom
