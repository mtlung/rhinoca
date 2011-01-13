#include "pch.h"
#include "canvas2dcontext.h"
#include "image.h"

namespace Dom {

JSClass Canvas2dContext::jsClass = {
	"Canvas2dContext", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub,  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub,  JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSBool drawImage(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	if(!JS_InstanceOf(cx, obj, &Canvas2dContext::jsClass, argv)) return JS_FALSE;
	Canvas2dContext* self = reinterpret_cast<Canvas2dContext*>(JS_GetPrivate(cx, obj));
	if(!self) return JS_FALSE;

	JSObject* imgObj = NULL;
	JS_ValueToObject(cx, argv[0], &imgObj);

	// Determine the source is an image or a canvas
//	TexturePtr texture;
	if(JS_InstanceOf(cx, imgObj, &Image::jsClass, NULL)) {
		Image* img = reinterpret_cast<Image*>(JS_GetPrivate(cx ,imgObj));
		img = img;
//		texture = img->texture;
	}
	if(JS_InstanceOf(cx, imgObj, &Canvas::jsClass, NULL)) {
		Canvas* otherCanvas = reinterpret_cast<Canvas*>(JS_GetPrivate(cx ,imgObj));
//		texture = otherCanvas->renderTarget->textures[0];
	}

//	double sx, sy, sw, sh;	// Source x, y, width and height
//	double dx, dy, dw, dh;	// Dest x, y, width and height

	return JS_TRUE;
}

static JSFunctionSpec methods[] = {
	{"drawImage", drawImage, 5,0,0},
	{0}
};

Canvas2dContext::Canvas2dContext(Canvas* c)
	: Context(c)
{
}

Canvas2dContext::~Canvas2dContext()
{
	releaseGcRoot();
}

void Canvas2dContext::bind(JSContext* cx, JSObject* parent)
{
	ASSERT(!jsContext);
	jsContext = cx;
	jsObject = JS_NewObject(cx, &jsClass, NULL, NULL);
	VERIFY(JS_SetPrivate(cx, jsObject, this));
	VERIFY(JS_DefineFunctions(cx, jsObject, methods));
	addReference();
}

}	// namespace Dom
