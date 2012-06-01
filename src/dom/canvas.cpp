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
		roVerify(JS_SetReservedSlot(cx, *self->context, 0, *self));
		roVerify(JS_SetReservedSlot(cx, *self, 0, *self->context));
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
	, _useRenderTarget(false)
	, _width(0), _height(0)
{
}

HTMLCanvasElement::HTMLCanvasElement(Rhinoca* rh, unsigned width, unsigned height, bool frontBufferOnly)
	: Element(rh)
	, context(NULL)
	, _useRenderTarget(true)//(!frontBufferOnly)
	, _width(width), _height(height)
{
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
	roAssert(!jsContext);
	jsContext = cx;
	jsObject = JS_NewObject(cx, &jsClass, Element::createPrototype(), parent);
	roVerify(JS_SetPrivate(cx, *this, this));
	roVerify(JS_DefineFunctions(cx, *this, methods));
	roVerify(JS_DefineProperties(cx, *this, properties));
	addReference();	// releaseReference() in JsBindable::finalize()
}

void HTMLCanvasElement::createContext(const char* ctxName)
{
	if(context) return;

	if(roStrCaseCmp(ctxName, "2d") == 0) {
		CanvasRenderingContext2D* ctx2d = new CanvasRenderingContext2D(this);
		context = ctx2d;
		context->addReference();	// releaseReference() in ~HTMLCanvasElement()

		if(_useRenderTarget)
			ctx2d->_canvas.initTargetTexture(_width, _height);

		ctx2d->_canvas.clearRect(0, 0, (float)width(), (float)height());
	}
}

void HTMLCanvasElement::render(CanvasRenderingContext2D* ctx)
{
	Element::render(ctx);

	Window* window = ownerDocument()->window();
	HTMLCanvasElement* vc = window->virtualCanvas;

	CanvasRenderingContext2D* ctx2d = dynamic_cast<CanvasRenderingContext2D*>(ctx);
	CanvasRenderingContext2D* self2d = dynamic_cast<CanvasRenderingContext2D*>(context);

	if(self2d && ctx2d)
		if(ro::Texture* tex = self2d->_canvas.targetTexture.get())
			ctx2d->_canvas.drawImage(tex->handle, 0, 0);
}

void HTMLCanvasElement::registerClass(JSContext* cx, JSObject* parent)
{
	roVerify(JS_InitClass(cx, parent, NULL, &jsClass, &construct, 0, NULL, NULL, NULL, NULL));
}

static const ro::ConstString _tagName = "CANVAS";

Element* HTMLCanvasElement::factoryCreate(Rhinoca* rh, const char* type, XmlParser* parser)
{
	if(roStrCaseCmp(type, _tagName) != 0) return NULL;

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

ro::Texture* HTMLCanvasElement::texture()
{
	if(!context) return NULL;
	return context->texture();
}

void HTMLCanvasElement::setWidth(unsigned w)
{
	_width = w;
	if(context) context->setWidth(w);
}

void HTMLCanvasElement::setHeight(unsigned h)
{
	_height = h;
	if(context) context->setHeight(h);
}

const ro::ConstString& HTMLCanvasElement::tagName() const
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
