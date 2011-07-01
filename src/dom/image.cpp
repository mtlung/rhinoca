#include "pch.h"
#include "image.h"
#include "document.h"
#include "../context.h"
#include "../path.h"
#include "../resource.h"
#include "../xmlparser.h"
#include "../render/texture.h"
#include <string.h>	// for strcasecmp()

using namespace Render;

namespace Dom {

JSClass HTMLImageElement::jsClass = {
	"HTMLImageElement", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub, JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
};

static void triggerLoadEvent(HTMLImageElement* self, const char* event)
{
	Dom::Event* ev = new Dom::Event;
	ev->type = event;
	ev->bubbles = false;
	ev->target = self;
	ev->bind(self->jsContext, NULL);
	self->dispatchEvent(ev);
}

static void onReadyCallback(TaskPool* taskPool, void* userData)
{
	HTMLImageElement* self = reinterpret_cast<HTMLImageElement*>(userData);
	triggerLoadEvent(self, (self->texture->state == Texture::Aborted) ? "error" : "ready");
	self->releaseGcRoot();
}

static void onLoadCallback(TaskPool* taskPool, void* userData)
{
	HTMLImageElement* self = reinterpret_cast<HTMLImageElement*>(userData);
	triggerLoadEvent(self, (self->texture->state == Texture::Aborted) ? "error" : "load");
	self->releaseGcRoot();
}

static JSBool getSrc(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	HTMLImageElement* self = getJsBindable<HTMLImageElement>(cx, obj);
	*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, self->texture->uri()));
	return JS_TRUE;
}

static JSBool setSrc(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	HTMLImageElement* self = getJsBindable<HTMLImageElement>(cx, obj);

	JsString str(cx, *vp);
	if(!str) return JS_FALSE;

	self->setSrc(str.c_str());

	return JS_TRUE;
}

static JSBool getWidth(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	HTMLImageElement* self = getJsBindable<HTMLImageElement>(cx, obj);
	*vp = INT_TO_JSVAL(self->width()); return JS_TRUE;
}

static JSBool setWidth(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	HTMLImageElement* self = getJsBindable<HTMLImageElement>(cx, obj);
	int32 width;
	if(!JS_ValueToInt32(cx, *vp, &width)) return JS_FALSE;
	self->_width = width; return JS_TRUE;
}

static JSBool getHeight(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	HTMLImageElement* self = getJsBindable<HTMLImageElement>(cx, obj);
	*vp = INT_TO_JSVAL(self->height()); return JS_TRUE;
}

static JSBool setHeight(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	HTMLImageElement* self = getJsBindable<HTMLImageElement>(cx, obj);
	int32 height;
	if(!JS_ValueToInt32(cx, *vp, &height)) return JS_FALSE;
	self->_height = height; return JS_TRUE;
}

static JSBool naturalWidth(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	HTMLImageElement* self = getJsBindable<HTMLImageElement>(cx, obj);
	*vp = INT_TO_JSVAL(self->naturalWidth()); return JS_TRUE;
}

static JSBool naturalHeight(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	HTMLImageElement* self = getJsBindable<HTMLImageElement>(cx, obj);
	*vp = INT_TO_JSVAL(self->naturalHeight()); return JS_TRUE;
}

static JSBool complete(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	HTMLImageElement* self = getJsBindable<HTMLImageElement>(cx, obj);
	bool ret = self->texture && self->texture->state == Texture::Loaded;
	*vp = BOOLEAN_TO_JSVAL(ret); return JS_TRUE;
}

static const char* _eventAttributeTable[] = {
	"onready",
	"onload",
	"onerror"
};

static JSBool getEventAttribute(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	HTMLImageElement* self = getJsBindable<HTMLImageElement>(cx, obj);
	int32 idx = JSID_TO_INT(id);

	*vp = self->getEventListenerAsAttribute(cx, _eventAttributeTable[idx]);
	return JS_TRUE;
}

static JSBool setEventAttribute(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	HTMLImageElement* self = getJsBindable<HTMLImageElement>(cx, obj);
	int32 idx = JSID_TO_INT(id);

	self->addEventListenerAsAttribute(cx, _eventAttributeTable[idx], *vp);

	// In case the Image is already loaded when we assign the callback, invoke the callback immediately
	Dom::Event* ev = NULL;

	if(self->texture) {
		const Texture::State state = self->texture->state;

		if(state == Texture::Ready && JSID_TO_INT(id) == 0) goto Dispatch;
		if(state == Texture::Loaded && JSID_TO_INT(id) == 1) goto Dispatch;
		if(state == Texture::Aborted && JSID_TO_INT(id) == 2) goto Dispatch;
	}
	goto Return;

Dispatch:
	ev = new Dom::Event;
	ev->type = _eventAttributeTable[JSID_TO_INT(id)] + 2;	// +2 to skip the "on" ("onload" -> "load")
	ev->bubbles = false;
	ev->target = self;
	ev->bind(cx, NULL);
	self->dispatchEvent(ev);

Return:
	return JS_PropertyStub(cx, obj, id, vp);
}

static JSPropertySpec properties[] = {
	{"src", 0, JsBindable::jsPropFlags, getSrc, setSrc},
	{"width", 0, JsBindable::jsPropFlags, getWidth, setWidth},
	{"height", 0, JsBindable::jsPropFlags, getHeight, setHeight},
	{"naturalWidth", 0, JSPROP_READONLY | JsBindable::jsPropFlags, naturalWidth, JS_StrictPropertyStub},
	{"naturalHeight", 0, JSPROP_READONLY | JsBindable::jsPropFlags, naturalHeight, JS_StrictPropertyStub},
	{"complete", 0, JSPROP_READONLY | JsBindable::jsPropFlags, complete, JS_StrictPropertyStub},

	// Event attributes
	{_eventAttributeTable[0], 0, JsBindable::jsPropFlags, getEventAttribute, setEventAttribute},
	{_eventAttributeTable[1], 1, JsBindable::jsPropFlags, getEventAttribute, setEventAttribute},
	{_eventAttributeTable[2], 2, JsBindable::jsPropFlags, getEventAttribute, setEventAttribute},
	{0}
};

static JSBool construct(JSContext* cx, uintN argc, jsval* vp)
{
	if(!JS_IsConstructing(cx, vp)) return JS_FALSE;	// Not called as constructor? (called without new)

	Rhinoca* rh = reinterpret_cast<Rhinoca*>(JS_GetContextPrivate(cx));
	HTMLImageElement* img = new HTMLImageElement(rh);
	img->bind(cx, NULL);

	JS_RVAL(cx, vp) = *img;

	return JS_TRUE;
}

static JSFunctionSpec methods[] = {
	{0}
};

HTMLImageElement::HTMLImageElement(Rhinoca* rh)
	: Element(rh)
	, texture(NULL)
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
	VERIFY(JS_InitClass(cx, parent, NULL, &jsClass, &construct, 0, NULL, NULL, NULL, NULL));
}

Element* HTMLImageElement::factoryCreate(Rhinoca* rh, const char* type, XmlParser* parser)
{
	HTMLImageElement* img = strcasecmp(type, "IMG") == 0 ? new HTMLImageElement(rh) : NULL;
	return img;
}

void HTMLImageElement::setSrc(const char* uri)
{
	Path path;
	fixRelativePath(uri, rhinoca->documentUrl.c_str(), path);

	ResourceManager& mgr = rhinoca->resourceManager;
	texture = mgr.loadAs<Texture>(path.c_str());

	// Register callbacks
	if(texture) {
		int tId = TaskPool::threadId();

		// NOTE: The ordering of registering 'ready' and 'load' are revered
		mgr.taskPool->addCallback(texture->taskLoaded, onLoadCallback, this, tId);
		mgr.taskPool->addCallback(texture->taskReady, onReadyCallback, this, tId);

		// Prevent HTMLImageElement begging GC before the callbacks finished.
		addGcRoot();
		addGcRoot();
	}
	else
		triggerLoadEvent(this, "error");
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
