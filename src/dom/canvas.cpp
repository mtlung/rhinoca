#include "pch.h"
#include "canvas.h"
#include "canvas2dcontext.h"
#include "../context.h"
#include "../xmlparser.h"

using namespace Render;

namespace Dom {

JSClass Canvas::jsClass = {
	"Canvas", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub,  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub,  JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSBool getWidth(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	Canvas* canvas = reinterpret_cast<Canvas*>(JS_GetPrivate(cx, obj));
	return JS_NewNumberValue(cx, canvas->width(), vp);
}

static JSBool getHeight(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	Canvas* canvas = reinterpret_cast<Canvas*>(JS_GetPrivate(cx, obj));
	return JS_NewNumberValue(cx, canvas->height(), vp);
}

static JSBool setWidth(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	// TODO: Implement
	return JS_TRUE;
}

static JSBool setHeight(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	// TODO: Implement
	return JS_TRUE;
}

static JSPropertySpec properties[] = {
	{"width", 0, 0, getWidth, setWidth},
	{"height", 0, 0, getHeight, setHeight},
	{0}
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
	VERIFY(JS_DefineProperties(cx, jsObject, properties));
	addReference();
}

void Canvas::bindFramebuffer()
{
	_framebuffer.bind();
}

void Canvas::unbindFramebuffer()
{
	_framebuffer.unbind();
}

Element* Canvas::factoryCreate(Rhinoca* rh, const char* type, XmlParser* parser)
{
	if(strcasecmp(type, "Canvas") != 0) return NULL;

	Canvas* canvas = new Canvas;

	// NOTE: Assume html specified canvas is visible
	if(parser)
		canvas->_framebuffer.useExternal(rh->renderContex);

	/// The default size of a canvas is 300 x 150 as described by the standard
	float w = 300, h = 150;

	if(parser) {
		w = parser->attributeValueAsFloat("width", w);
		h = parser->attributeValueAsFloat("height", h);
	}

	canvas->setWidth((unsigned)w);
	canvas->setHeight((unsigned)h);

	return canvas;
}

Texture* Canvas::texture()
{
	return _framebuffer.texture.get();
}

void Canvas::setWidth(unsigned w)
{
	if(w == width()) return;
	_framebuffer.createTexture(w, height());
}

void Canvas::setHeight(unsigned h)
{
	if(h == height()) return;
	_framebuffer.createTexture(width(), h);
}

Canvas::Context::Context(Canvas* c)
	: canvas(c)
{
}

Canvas::Context::~Context()
{
}

}	// namespace Dom
