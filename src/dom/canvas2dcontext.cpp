#include "pch.h"
#include "canvas2dcontext.h"

namespace Dom {

JSClass Canvas2dContext::jsClass = {
	"Canvas2dContext", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub,  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub,  JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
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
	addReference();
}

}	// namespace Dom
