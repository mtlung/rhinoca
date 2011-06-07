#include "pch.h"
#include "image.h"
#include "document.h"
#include "../context.h"
#include "../path.h"
#include "../resource.h"
#include "../xmlparser.h"
#include "../render/texture.h"

using namespace Render;

namespace Dom {

JSClass HTMLImageElement::jsClass = {
	"HTMLImageElement", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub, JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
};

static void onReadyCallback(TaskPool* taskPool, void* userData)
{
	HTMLImageElement* self = reinterpret_cast<HTMLImageElement*>(userData);

	Dom::Event* ev = new Dom::Event;
	ev->type = (self->texture->state == Texture::Aborted) ? "error" : "ready";
	ev->bubbles = false;
	ev->target = self;
	ev->bind(self->jsContext, NULL);

	self->dispatchEvent(ev);
	self->releaseGcRoot();
}

static void onLoadCallback(TaskPool* taskPool, void* userData)
{
	HTMLImageElement* self = reinterpret_cast<HTMLImageElement*>(userData);

	Dom::Event* ev = new Dom::Event;
	ev->type = (self->texture->state == Texture::Aborted) ? "error" : "load";
	ev->bubbles = false;
	ev->target = self;
	ev->bind(self->jsContext, NULL);

	self->dispatchEvent(ev);
	self->releaseGcRoot();
}

static JSBool getSrc(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	HTMLImageElement* self = reinterpret_cast<HTMLImageElement*>(JS_GetPrivate(cx, obj));
	*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, self->texture->uri().c_str()));
	return JS_TRUE;
}

static JSBool setSrc(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	HTMLImageElement* self = reinterpret_cast<HTMLImageElement*>(JS_GetPrivate(cx, obj));

	JSString* jss = JS_ValueToString(cx, *vp);
	if(!jss) return JS_FALSE;
	char* str = JS_GetStringBytes(jss);

	self->setSrc(self->ownerDocument->rhinoca, str);

	return JS_TRUE;
}

static JSBool getWidth(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	HTMLImageElement* self = reinterpret_cast<HTMLImageElement*>(JS_GetPrivate(cx, obj));
	*vp = INT_TO_JSVAL(self->width()); return JS_TRUE;
}

static JSBool setWidth(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	HTMLImageElement* self = reinterpret_cast<HTMLImageElement*>(JS_GetPrivate(cx, obj));
	int32 width;
	if(!JS_ValueToInt32(cx, *vp, &width)) return JS_FALSE;
	self->_width = width; return JS_TRUE;
}

static JSBool getHeight(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	HTMLImageElement* self = reinterpret_cast<HTMLImageElement*>(JS_GetPrivate(cx, obj));
	*vp = INT_TO_JSVAL(self->height()); return JS_TRUE;
}

static JSBool setHeight(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	HTMLImageElement* self = reinterpret_cast<HTMLImageElement*>(JS_GetPrivate(cx, obj));
	int32 height;
	if(!JS_ValueToInt32(cx, *vp, &height)) return JS_FALSE;
	self->_height = height; return JS_TRUE;
}

static JSBool naturalWidth(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	HTMLImageElement* self = reinterpret_cast<HTMLImageElement*>(JS_GetPrivate(cx, obj));
	*vp = INT_TO_JSVAL(self->naturalWidth()); return JS_TRUE;
}

static JSBool naturalHeight(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	HTMLImageElement* self = reinterpret_cast<HTMLImageElement*>(JS_GetPrivate(cx, obj));
	*vp = INT_TO_JSVAL(self->naturalHeight()); return JS_TRUE;
}

static JSBool complete(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	HTMLImageElement* self = reinterpret_cast<HTMLImageElement*>(JS_GetPrivate(cx, obj));
	bool ret = self->texture && self->texture->state == Texture::Loaded;
	*vp = BOOLEAN_TO_JSVAL(ret); return JS_TRUE;
}

static const char* _eventAttributeTable[] = {
	"onready",
	"onload",
	"onerror"
};

static JSBool setEventAttribute(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	HTMLImageElement* self = reinterpret_cast<HTMLImageElement*>(JS_GetPrivate(cx, obj));
	id = id / 2 + 0;	// Account for having both get and set functions

	self->addEventListenerAsAttribute(cx, _eventAttributeTable[id], *vp);

	// In case the Image is already loaded when we assign the callback, invoke the callback immediately
	Dom::Event* ev = NULL;

	if(!self->texture) goto Return;

	const Texture::State state = self->texture->state;

	if(state == Texture::Ready && id == 0) goto Dispatch;
	if(state == Texture::Loaded && id == 1) goto Dispatch;
	if(state == Texture::Aborted && id == 2) goto Dispatch;
	goto Return;

Dispatch:
	ev = new Dom::Event;
	ev->type = _eventAttributeTable[id] + 2;	// +2 to skip the "on" ("onload" -> "load")
	ev->bubbles = false;
	ev->target = self;
	ev->bind(cx, NULL);
	self->dispatchEvent(ev);

Return:
	return JS_PropertyStub(cx, obj, id, vp);
}

static JSPropertySpec properties[] = {
	{"src", 0, 0, getSrc, setSrc},
	{"width", 0, 0, getWidth, setWidth},
	{"height", 0, 0, getHeight, setHeight},
	{"naturalWidth", 0, 0, naturalWidth, JS_PropertyStub},
	{"naturalHeight", 0, 0, naturalHeight, JS_PropertyStub},
	{"complete", 0, 0, complete, JS_PropertyStub},

	// Event attributes
	{_eventAttributeTable[0], 0, 0, JS_PropertyStub, setEventAttribute},
	{_eventAttributeTable[1], 1, 0, JS_PropertyStub, setEventAttribute},
	{_eventAttributeTable[2], 2, 0, JS_PropertyStub, setEventAttribute},
	{0}
};

static JSBool construct(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	if(!JS_IsConstructing(cx)) return JS_FALSE;	// Not called as constructor? (called without new)

	HTMLImageElement* img = new HTMLImageElement;
	img->bind(cx, NULL);

	Rhinoca* rh = reinterpret_cast<Rhinoca*>(JS_GetContextPrivate(cx));
	img->ownerDocument = rh->domWindow->document;

	*rval = *img;

	return JS_TRUE;
}

static JSFunctionSpec methods[] = {
	{0}
};

HTMLImageElement::HTMLImageElement()
	: texture(NULL)
	, _width(-1), _height(-1)
{
}

HTMLImageElement::~HTMLImageElement()
{
}

void HTMLImageElement::bind(JSContext* cx, JSObject* parent)
{
	ASSERT(!jsContext);
	jsContext = cx;
	jsObject = JS_NewObject(cx, &jsClass, Element::createPrototype(), parent);
	VERIFY(JS_SetPrivate(cx, *this, this));
	VERIFY(JS_DefineFunctions(cx, *this, methods));
	VERIFY(JS_DefineProperties(cx, *this, properties));
	addReference();
}

void HTMLImageElement::registerClass(JSContext* cx, JSObject* parent)
{
	JS_InitClass(cx, parent, NULL, &jsClass, &construct, 0, NULL, NULL, NULL, NULL);
}

Element* HTMLImageElement::factoryCreate(Rhinoca* rh, const char* type, XmlParser* parser)
{
	HTMLImageElement* img = strcasecmp(type, "IMG") == 0 ? new HTMLImageElement : NULL;
	if(!img) return NULL;

	if(!img->jsContext)
		img->bind(rh->jsContext, NULL);

	// Parse HTMLImageElement attributes
	if(const char* s = parser->attributeValueIgnoreCase("src"))
		img->setSrc(rh, s);

	return img;
}

void HTMLImageElement::setSrc(Rhinoca* rh, const char* uri)
{
	Path path;
	fixRelativePath(uri, rh->documentUrl.c_str(), path);

	ResourceManager& mgr = rh->resourceManager;
	texture = mgr.loadAs<Texture>(path.c_str());

	// Register callbacks
	if(texture) {
		int tId = TaskPool::threadId();
		mgr.taskPool->addCallback(texture->taskReady, onReadyCallback, this, tId);
		mgr.taskPool->addCallback(texture->taskLoaded, onLoadCallback, this, tId);

		// Prevent HTMLImageElement begging GC before the callback finished.
		addGcRoot();
		addGcRoot();
	}
	else
		print(rh, "Failed to load: '%s'\n", path.c_str());
}

rhuint HTMLImageElement::width() const
{
	return _width < 0 ? naturalWidth() : _width;
}

rhuint HTMLImageElement::height() const
{
	return _height < 0 ? naturalHeight() : _height;
}

rhuint HTMLImageElement::naturalWidth() const
{
	return texture ? texture->width : 0;
}

rhuint HTMLImageElement::naturalHeight() const
{
	return texture ? texture->height : 0;
}

static const FixString _tagName = "IMG";

const FixString& HTMLImageElement::tagName() const
{
	return _tagName;
}

}	// namespace Dom
