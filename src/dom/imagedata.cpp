#include "pch.h"
#include "ImageData.h"

#define XP_WIN
#include "../../thirdParty/SpiderMonkey/jstypedarray.h"

using namespace Render;

namespace Dom {

JSClass ImageData::jsClass = {
	"ImageData", JSCLASS_HAS_PRIVATE | JSCLASS_HAS_RESERVED_SLOTS(1),
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub, JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSFunctionSpec methods[] = {
	{0}
};

static JSBool width(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	ImageData* self = getJsBindable<ImageData>(cx, obj);
	*vp = INT_TO_JSVAL((int)self->width);
	return JS_TRUE;
}

static JSBool height(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	ImageData* self = getJsBindable<ImageData>(cx, obj);
	*vp = INT_TO_JSVAL((int)self->height);
	return JS_TRUE;
}

static JSBool data(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	ImageData* self = getJsBindable<ImageData>(cx, obj);
	*vp = OBJECT_TO_JSVAL(self->array);
	return JS_TRUE;
}

static JSPropertySpec properties[] = {
	{"width", 0, JSPROP_READONLY | JsBindable::jsPropFlags, width, JS_StrictPropertyStub},
	{"height", 0, JSPROP_READONLY | JsBindable::jsPropFlags, height, JS_StrictPropertyStub},
	{"data", 0, JSPROP_READONLY | JsBindable::jsPropFlags, data, JS_StrictPropertyStub},
	{0}
};

ImageData::ImageData()
	: width(0), height(0)
	, array(NULL)
{
}

ImageData::~ImageData()
{
}

void ImageData::bind(JSContext* cx, JSObject* parent)
{
	RHASSERT(!jsContext);
	jsContext = cx;
	jsObject = JS_NewObject(cx, &jsClass, NULL, parent);
	RHVERIFY(JS_SetPrivate(cx, *this, this));
	RHVERIFY(JS_DefineFunctions(cx, *this, methods));
	RHVERIFY(JS_DefineProperties(cx, *this, properties));
	addReference();	// releaseReference() in JsBindable::finalize()
}

void ImageData::init(JSContext* cx, unsigned w, unsigned h, const unsigned char* rawData)
{
	bind(cx, NULL);

	width = w;
	height = h;

	RHASSERT(!array);
	array = js_CreateTypedArray(cx, js::TypedArray::TYPE_UINT8_CLAMPED, w * h * 4);
	RHVERIFY(JS_SetReservedSlot(cx, *this, 0, OBJECT_TO_JSVAL(array)));
}

rhbyte* ImageData::rawData()
{
	js::TypedArray* a = js::TypedArray::fromJSObject(array);
	return (rhbyte*)(a ? a->data : NULL);
}

}	// namespace Dom
