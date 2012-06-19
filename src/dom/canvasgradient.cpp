#include "pch.h"
#include "canvasgradient.h"
#include "color.h"
#include "../../roar/render/roCanvas.h"

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
	roVerify(JS_ValueToNumber(cx, JS_ARGV0, &t));

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
{
}

CanvasGradient::~CanvasGradient()
{
	ro::Canvas::destroyGradient(handle);
}

void CanvasGradient::bind(JSContext* cx, JSObject* parent)
{
	roAssert(!jsContext);
	jsContext = cx;
	jsObject = JS_NewObject(cx, &jsClass, NULL, parent);
	roVerify(JS_SetPrivate(cx, *this, this));
	roVerify(JS_DefineFunctions(cx, *this, methods));
	addReference();	// releaseReference() in JsBindable::finalize()
}

void CanvasGradient::createLinear(float xStart, float yStart, float xEnd, float yEnd)
{
	if(handle) ro::Canvas::destroyGradient(handle);
	handle = ro::Canvas::createLinearGradient(xStart, yStart, xEnd, yEnd);
}

void CanvasGradient::createRadial(float xStart, float yStart, float radiusStart, float xEnd, float yEnd, float radiusEnd)
{
	if(handle) ro::Canvas::destroyGradient(handle);
	handle = ro::Canvas::createRadialGradient(xStart, yStart, radiusStart, xEnd, yEnd, radiusEnd);
}

void CanvasGradient::addColorStop(float offset, const char* color)
{
	Color c;
	c.parse(color);
	ro::Canvas::addGradientColorStop(handle, offset, c.r, c.g, c.b, c.a);
}

}	// namespace Dom
