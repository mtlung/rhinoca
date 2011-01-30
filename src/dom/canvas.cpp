#include "pch.h"
#include "canvas.h"
#include "canvas2dcontext.h"
#include "../context.h"
#include "../xmlparser.h"

using namespace Render;

namespace Dom {

JSClass HTMLCanvasElement::jsClass = {
	"HTMLCanvasElement", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub,  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub,  JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSBool getWidth(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	HTMLCanvasElement* self = reinterpret_cast<HTMLCanvasElement*>(JS_GetPrivate(cx, obj));
	*vp = INT_TO_JSVAL(self->width()); return JS_TRUE;
}

static JSBool getHeight(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	HTMLCanvasElement* self = reinterpret_cast<HTMLCanvasElement*>(JS_GetPrivate(cx, obj));
	*vp = INT_TO_JSVAL(self->height()); return JS_TRUE;
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

static JSBool construct(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	if(!JS_IsConstructing(cx)) return JS_FALSE;	// Not called as constructor? (called without new)

	HTMLCanvasElement* img = new HTMLCanvasElement;
	img->bind(cx, NULL);

	Rhinoca* rh = reinterpret_cast<Rhinoca*>(JS_GetContextPrivate(cx));
	img->ownerDocument = rh->domWindow->document;

	*rval = OBJECT_TO_JSVAL(img->jsObject);

	return JS_TRUE;
}

static JSBool getContext(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	HTMLCanvasElement* self = reinterpret_cast<HTMLCanvasElement*>(JS_GetPrivate(cx, obj));

	JSString* jss = JS_ValueToString(cx, argv[0]);
	char* str = JS_GetStringBytes(jss);

	*rval = JSVAL_VOID;

	if(strcasecmp(str, "2d") == 0) {
		if(!self->context) {
			self->context = new CanvasRenderingContext2D(self);
			self->context->bind(cx, NULL);
			self->context->addGcRoot();	// releaseGcRoot() in ~HTMLCanvasElement()
		}
		*rval = OBJECT_TO_JSVAL(self->context->jsObject);
	}

	return JS_TRUE;
}

static JSFunctionSpec methods[] = {
	{"getContext", getContext, 1,0,0},
	{0}
};

HTMLCanvasElement::HTMLCanvasElement()
	: context(NULL)
{
}

HTMLCanvasElement::~HTMLCanvasElement()
{
	if(context) {
		context->canvas = NULL;
		context->releaseGcRoot();
	}
}

void HTMLCanvasElement::bind(JSContext* cx, JSObject* parent)
{
	ASSERT(!jsContext);
	jsContext = cx;
	jsObject = JS_NewObject(cx, &jsClass, Element::createPrototype(), NULL);
	VERIFY(JS_SetPrivate(cx, jsObject, this));
	VERIFY(JS_DefineFunctions(cx, jsObject, methods));
	VERIFY(JS_DefineProperties(cx, jsObject, properties));
	addReference();
}

void HTMLCanvasElement::bindFramebuffer()
{
	_framebuffer.bind();
}

void HTMLCanvasElement::unbindFramebuffer()
{
	_framebuffer.unbind();
}

void HTMLCanvasElement::registerClass(JSContext* cx, JSObject* parent)
{
	JS_InitClass(cx, parent, NULL, &jsClass, &construct, 0, NULL, NULL, NULL, NULL);
}

Element* HTMLCanvasElement::factoryCreate(Rhinoca* rh, const char* type, XmlParser* parser)
{
	if(strcasecmp(type, "CANVAS") != 0) return NULL;

	HTMLCanvasElement* canvas = new HTMLCanvasElement;

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

Texture* HTMLCanvasElement::texture()
{
	return _framebuffer.texture.get();
}

void HTMLCanvasElement::setWidth(unsigned w)
{
	if(w == width()) return;
	_framebuffer.createTexture(w, height());
}

void HTMLCanvasElement::setHeight(unsigned h)
{
	if(h == height()) return;
	_framebuffer.createTexture(width(), h);
}

const char* HTMLCanvasElement::tagName() const
{
	return "CANVAS";
}

HTMLCanvasElement::Context::Context(HTMLCanvasElement* c)
	: canvas(c)
{
}

HTMLCanvasElement::Context::~Context()
{
}

}	// namespace Dom
