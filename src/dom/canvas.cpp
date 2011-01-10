#include "pch.h"
#include "canvas.h"
#include "canvas2dcontext.h"

namespace Dom {

JSClass Canvas::jsClass = {
	"Canvas", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub,  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub,  JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSBool getContext(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	Canvas* self = reinterpret_cast<Canvas*>(JS_GetPrivate(cx, obj));

	JSString* jss = JS_ValueToString(cx, argv[0]);
	char* str = JS_GetStringBytes(jss);

	*rval = JSVAL_VOID;

	if(strcasecmp(str, "2d") == 0) {
		self->context = new Canvas2dContext(self);
		self->context->bind(cx, NULL);
		self->context->addGcRoot();
		*rval = OBJECT_TO_JSVAL(self->context->jsObject);
	}

	return JS_TRUE;
}

static JSFunctionSpec methods[] = {
	{"getContext", getContext, 1,0,0},
	{0}
};

Canvas::Canvas()
	: context(NULL)
{
}

Canvas::~Canvas()
{
	if(context) {
		context->canvas = NULL;
		context->releaseGcRoot();
	}
}

void Canvas::bind(JSContext* cx, JSObject* parent)
{
	ASSERT(!jsContext);
	jsContext = cx;
	jsObject = JS_NewObject(cx, &jsClass, Node::createPrototype(), NULL);
	VERIFY(JS_SetPrivate(cx, jsObject, this));
	VERIFY(JS_DefineFunctions(cx, jsObject, methods));
	addReference();
}

Element* Canvas::factoryCreate(const char* type)
{
	return strcasecmp(type, "Canvas") == 0 ? new Canvas : NULL;
}

void Canvas::setWidth()
{
}

void Canvas::setHeight()
{
}

Canvas::Context::Context(Canvas* c)
	: canvas(c)
{
}

Canvas::Context::~Context()
{
}

}	// namespace Dom
