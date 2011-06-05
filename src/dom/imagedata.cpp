#include "pch.h"
#include "ImageData.h"
#include "canvaspixelarray.h"

using namespace Render;

namespace Dom {

JSClass ImageData::jsClass = {
	"ImageData", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub, JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSFunctionSpec methods[] = {
	{0}
};

static JSBool width(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	ImageData* self = reinterpret_cast<ImageData*>(JS_GetPrivate(cx, obj));
	*vp = INT_TO_JSVAL((int)self->width);
	return JS_TRUE;
}

static JSBool height(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	ImageData* self = reinterpret_cast<ImageData*>(JS_GetPrivate(cx, obj));
	*vp = INT_TO_JSVAL((int)self->height);
	return JS_TRUE;
}

static JSBool data(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	ImageData* self = reinterpret_cast<ImageData*>(JS_GetPrivate(cx, obj));
	*vp = *self->data;
	return JS_TRUE;
}

static JSPropertySpec properties[] = {
	{"width", 0, JSPROP_READONLY, width, JS_PropertyStub},
	{"height", 0, JSPROP_READONLY, height, JS_PropertyStub},
	{"data", 0, JSPROP_READONLY, data, JS_PropertyStub},
	{0}
};

ImageData::ImageData()
	: width(0), height(0)
	, data(NULL)
{
}

ImageData::~ImageData()
{
	if(data)
		data->releaseGcRoot();
}

void ImageData::bind(JSContext* cx, JSObject* parent)
{
	ASSERT(!jsContext);
	jsContext = cx;
	jsObject = JS_NewObject(cx, &jsClass, NULL, parent);
	VERIFY(JS_SetPrivate(cx, *this, this));
	VERIFY(JS_DefineFunctions(cx, *this, methods));
	VERIFY(JS_DefineProperties(cx, *this, properties));
	addReference();	// releaseReference() in JsBindable::finalize()

	data->bind(cx, NULL);
	data->addGcRoot();	// releaseGcRoot() in ~ImageData()
}

void ImageData::init(unsigned w, unsigned h, const unsigned char* rawData)
{
	width = w;
	height = h;

	data = new CanvasPixelArray;
	data->length = w * h * 4;
	data->rawData = (unsigned char*)rhinoca_malloc(data->length);
}

}	// namespace Dom
