#include "pch.h"
#include "canvaspixelarray.h"

namespace Dom {

static JSBool getProperty(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	CanvasPixelArray* self = getJsBindable<CanvasPixelArray>(cx, obj);

	int32 index = JSID_TO_INT(id);
	if(index < 0 || (unsigned)index >= self->length)
		return JS_FALSE;

	ASSERT(self->rawData);
	*vp = INT_TO_JSVAL((int)self->getAt(index));
	return JS_TRUE;
}

static JSBool setProperty(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	CanvasPixelArray* self = getJsBindable<CanvasPixelArray>(cx, obj);

	int32 index = JSID_TO_INT(id);
	if(index < 0 || (unsigned)index >= self->length)
		return JS_FALSE;

	ASSERT(self->rawData);
	int32 val;
	if(!JS_ValueToInt32(cx, *vp, &val)) return JS_FALSE;
	self->setAt(index, (unsigned char)val);
	return JS_TRUE;
}

JSClass CanvasPixelArray::jsClass = {
	"CanvasPixelArray", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, getProperty, setProperty,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub, JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSFunctionSpec methods[] = {
	{0}
};

static JSBool length(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	CanvasPixelArray* self = getJsBindable<CanvasPixelArray>(cx, obj);
	*vp = INT_TO_JSVAL((int)self->length);
	return JS_TRUE;
}

static JSPropertySpec properties[] = {
	{"length", 0, JSPROP_READONLY, length, JS_StrictPropertyStub},
	{0}
};

CanvasPixelArray::CanvasPixelArray()
	: length(0), rawData(NULL)
{
}

CanvasPixelArray::~CanvasPixelArray()
{
	rhinoca_free(rawData);
}

void CanvasPixelArray::bind(JSContext* cx, JSObject* parent)
{
	ASSERT(!jsContext);
	jsContext = cx;
	jsObject = JS_NewObject(cx, &jsClass, NULL, parent);
	VERIFY(JS_SetPrivate(cx, *this, this));
	VERIFY(JS_DefineFunctions(cx, *this, methods));
	VERIFY(JS_DefineProperties(cx, *this, properties));
	addReference();	// releaseReference() in JsBindable::finalize()
}

}	// namespace Dom
