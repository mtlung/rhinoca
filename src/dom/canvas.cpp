#include "pch.h"
#include "canvas.h"
#include "canvas2dcontext.h"
#include "document.h"
#include "../context.h"
#include "../xmlparser.h"
#include <string.h>

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
	HTMLCanvasElement* self = reinterpret_cast<HTMLCanvasElement*>(JS_GetPrivate(cx, obj));
	self->setWidth(JSVAL_TO_INT(*vp));
	return JS_TRUE;
}

static JSBool setHeight(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	HTMLCanvasElement* self = reinterpret_cast<HTMLCanvasElement*>(JS_GetPrivate(cx, obj));
	self->setHeight(JSVAL_TO_INT(*vp));
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

	*rval = *img;

	return JS_TRUE;
}

static JSBool getContext(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	HTMLCanvasElement* self = reinterpret_cast<HTMLCanvasElement*>(JS_GetPrivate(cx, obj));

	JSString* jss = JS_ValueToString(cx, argv[0]);
	char* str = JS_GetStringBytes(jss);

	*rval = JSVAL_VOID;

	if(!self->context) {
		self->createContext(str);
		self->context->bind(cx, NULL);
		self->context->addGcRoot();	// releaseGcRoot() in ~HTMLCanvasElement()
	}

	if(self->context) {
		*rval = *self->context;
		return JS_TRUE;
	}

	return JS_FALSE;
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
		if(jsContext) context->releaseGcRoot();
		context->releaseReference();
	}
}

void HTMLCanvasElement::bind(JSContext* cx, JSObject* parent)
{
	ASSERT(!jsContext);
	jsContext = cx;
	jsObject = JS_NewObject(cx, &jsClass, Element::createPrototype(), parent);
	VERIFY(JS_SetPrivate(cx, *this, this));
	VERIFY(JS_DefineFunctions(cx, *this, methods));
	VERIFY(JS_DefineProperties(cx, *this, properties));
	addReference();
}

void HTMLCanvasElement::bindFramebuffer()
{
	_framebuffer.bind();
}

void HTMLCanvasElement::createTextureFrameBuffer(unsigned w, unsigned h)
{
	if(w > 0 && h > 0)
		_framebuffer.createTexture(w, h);
}

void HTMLCanvasElement::useExternalFrameBuffer(Rhinoca* rh)
{
	_framebuffer.useExternal(rh->renderContex);
}

void HTMLCanvasElement::createContext(const char* ctxName)
{
	if(context) return;

	if(strcasecmp(ctxName, "2d") == 0) {
		context = new CanvasRenderingContext2D(this);
		context->addReference();	// releaseReference() in ~HTMLCanvasElement()
	}
}

void HTMLCanvasElement::render()
{
	// No need to draw to the window, if 'frontBufferOnly' is set to true such that all
	// draw operation is already put on the window
	if(!_framebuffer.texture) return;

	DOMWindow* window = ownerDocument->window();
	HTMLCanvasElement* vc = window->virtualCanvas;

	CanvasRenderingContext2D* ctx = dynamic_cast<CanvasRenderingContext2D*>(vc->context);

	ctx->clearRect(0, 0, (float)width(), (float)height());
	ctx->drawImage(texture(), 0, 0);
}

void HTMLCanvasElement::registerClass(JSContext* cx, JSObject* parent)
{
	JS_InitClass(cx, parent, NULL, &jsClass, &construct, 0, NULL, NULL, NULL, NULL);
}

Element* HTMLCanvasElement::factoryCreate(Rhinoca* rh, const char* type, XmlParser* parser)
{
	if(strcasecmp(type, "CANVAS") != 0) return NULL;

	HTMLCanvasElement* canvas = new HTMLCanvasElement;

	/// The default size of a canvas is 300 x 150 as described by the standard
	float w = 300, h = 150;

	if(parser) {
		w = parser->attributeValueAsFloatIgnoreCase("width", w);
		h = parser->attributeValueAsFloatIgnoreCase("height", h);

		bool frontBufferOnly = parser->attributeValueAsBoolIgnoreCase("frontBufferOnly", false);
		if(frontBufferOnly)
			canvas->useExternalFrameBuffer(rh);
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
	if(!(_framebuffer.handle && !_framebuffer.texture))	// No need to resize external render target
		createTextureFrameBuffer(w, _framebuffer.height);
	_framebuffer.width = w;
}

void HTMLCanvasElement::setHeight(unsigned h)
{
	if(!(_framebuffer.handle && !_framebuffer.texture))	// No need to resize external render target
		createTextureFrameBuffer(_framebuffer.width, h);
	_framebuffer.height = h;
}

static const FixString _tagName = "CANVAS";

const FixString& HTMLCanvasElement::tagName() const
{
	return _tagName;
}

HTMLCanvasElement::Context::Context(HTMLCanvasElement* c)
	: canvas(c)
{
}

HTMLCanvasElement::Context::~Context()
{
}

}	// namespace Dom
