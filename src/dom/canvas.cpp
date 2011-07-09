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
	"HTMLCanvasElement", JSCLASS_HAS_PRIVATE | JSCLASS_HAS_RESERVED_SLOTS(1),
	JS_PropertyStub,  JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub,  JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSBool getWidth(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	HTMLCanvasElement* self = getJsBindable<HTMLCanvasElement>(cx, obj);
	*vp = INT_TO_JSVAL(self->width()); return JS_TRUE;
}

static JSBool getHeight(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	HTMLCanvasElement* self = getJsBindable<HTMLCanvasElement>(cx, obj);
	*vp = INT_TO_JSVAL(self->height()); return JS_TRUE;
}

static JSBool setWidth(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	HTMLCanvasElement* self = getJsBindable<HTMLCanvasElement>(cx, obj);
	int32 width;
	if(!JS_ValueToInt32(cx, *vp, &width)) return JS_FALSE;
	self->setWidth(width);
	return JS_TRUE;
}

static JSBool setHeight(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	HTMLCanvasElement* self = getJsBindable<HTMLCanvasElement>(cx, obj);
	int32 height;
	if(!JS_ValueToInt32(cx, *vp, &height)) return JS_FALSE;
	self->setHeight(height);
	return JS_TRUE;
}

static JSPropertySpec properties[] = {
	{"width", 0, JsBindable::jsPropFlags, getWidth, setWidth},
	{"height", 0, JsBindable::jsPropFlags, getHeight, setHeight},
	{0}
};

static JSBool construct(JSContext* cx, uintN argc, jsval* vp)
{
	if(!JS_IsConstructing(cx, vp)) return JS_FALSE;	// Not called as constructor? (called without new)

	Rhinoca* rh = reinterpret_cast<Rhinoca*>(JS_GetContextPrivate(cx));
	HTMLCanvasElement* img = new HTMLCanvasElement(rh);
	img->bind(cx, NULL);

	JS_RVAL(cx, vp) = *img;

	return JS_TRUE;
}

static JSBool getContext(JSContext* cx, uintN argc, jsval* vp)
{
	HTMLCanvasElement* self = getJsBindable<HTMLCanvasElement>(cx, vp);

	JsString jss(cx, JS_ARGV0);
	if(!jss) return JS_FALSE;

	if(!self->context) {
		self->createContext(jss.c_str());
		self->context->bind(cx, NULL);
		// Create mutual reference between Canvas and Canvas2DContext, to make sure
		// they live together in JSGC
		VERIFY(JS_SetReservedSlot(cx, *self->context, 0, *self));
		VERIFY(JS_SetReservedSlot(cx, *self, 0, *self->context));

	}

	if(self->context) {
		JS_RVAL(cx, vp) = *self->context;
		return JS_TRUE;
	}

	return JS_FALSE;
}

static JSFunctionSpec methods[] = {
	{"getContext", getContext, 1,0,},
	{0}
};

HTMLCanvasElement::HTMLCanvasElement(Rhinoca* rh)
	: Element(rh)
	, context(NULL)
{
}

HTMLCanvasElement::HTMLCanvasElement(Rhinoca* rh, unsigned width, unsigned height, bool frontBufferOnly)
	: Element(rh)
	, context(NULL)
{
	if(frontBufferOnly)
		useExternalFrameBuffer(rh);

	if(!(_framebuffer.handle && !_framebuffer.texture))	// No need to resize external render target
		createTextureFrameBuffer(width, height);

	_framebuffer.width = width;
	_framebuffer.height = height;
}

HTMLCanvasElement::~HTMLCanvasElement()
{
	if(context) {
		context->canvas = NULL;
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

	Window* window = ownerDocument()->window();
	HTMLCanvasElement* vc = window->virtualCanvas;

	CanvasRenderingContext2D* ctx = dynamic_cast<CanvasRenderingContext2D*>(vc->context);

	ctx->clearRect(0, 0, (float)width(), (float)height());
	ctx->drawImage(texture(), Render::Driver::SamplerState::MIN_MAG_LINEAR, 0, 0);
}

void HTMLCanvasElement::registerClass(JSContext* cx, JSObject* parent)
{
	VERIFY(JS_InitClass(cx, parent, NULL, &jsClass, &construct, 0, NULL, NULL, NULL, NULL));
}

Element* HTMLCanvasElement::factoryCreate(Rhinoca* rh, const char* type, XmlParser* parser)
{
	if(strcasecmp(type, "CANVAS") != 0) return NULL;

	/// The default size of a canvas is 300 x 150 as described by the standard
	float w = 300, h = 150;
	bool frontBufferOnly = false;

	if(parser) {
		w = parser->attributeValueAsFloatIgnoreCase("width", w);
		h = parser->attributeValueAsFloatIgnoreCase("height", h);

		frontBufferOnly = parser->attributeValueAsBoolIgnoreCase("frontBufferOnly", false);
	}

	HTMLCanvasElement* canvas = new HTMLCanvasElement(rh, (unsigned)w, (unsigned)h, frontBufferOnly);

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
