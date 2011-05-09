#include "pch.h"
#include "image.h"
#include "document.h"
#include "../context.h"
#include "../path.h"
#include "../resource.h"
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

	jsval rval;
	jsval closure;
	const char* event = (self->texture->state == Texture::Aborted) ? "onerror" : "onready";
	if(JS_GetProperty(self->jsContext, self->jsObject, event, &closure) && closure != JSVAL_VOID)
		JS_CallFunctionValue(self->jsContext, self->jsObject, closure, 0, NULL, &rval);

	self->releaseGcRoot();
}

static void onLoadCallback(TaskPool* taskPool, void* userData)
{
	HTMLImageElement* self = reinterpret_cast<HTMLImageElement*>(userData);

	jsval rval;
	jsval closure;
	const char* event = (self->texture->state == Texture::Aborted) ? "onerror" : "onload";
	if(JS_GetProperty(self->jsContext, self->jsObject, event, &closure) && closure != JSVAL_VOID)
		JS_CallFunctionValue(self->jsContext, self->jsObject, closure, 0, NULL, &rval);

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

	Path path;
	if(Path(str).hasRootDirectory())	// Absolute path
		path = str;
	else {
		// Relative path to the document
		path = self->ownerDocument->rhinoca->documentUrl.c_str();
		path = path.getBranchPath() / str;
	}

	ResourceManager& mgr = self->ownerDocument->rhinoca->resourceManager;
	self->texture = mgr.loadAs<Texture>(path.c_str());

	// Register callbacks
	if(self->texture) {
		int tId = TaskPool::threadId();
		mgr.taskPool->addCallback(self->texture->taskReady, onReadyCallback, self, tId);
		mgr.taskPool->addCallback(self->texture->taskLoaded, onLoadCallback, self, tId);

		// Prevent HTMLImageElement begging GC before the callback finished.
		self->addGcRoot();
		self->addGcRoot();
	}
	else
		print(self->ownerDocument->rhinoca, "Failed to load: '%s'\n", path.c_str());

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
	self->_width = JSVAL_TO_INT(vp); return JS_TRUE;
}

static JSBool getHeight(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	HTMLImageElement* self = reinterpret_cast<HTMLImageElement*>(JS_GetPrivate(cx, obj));
	*vp = INT_TO_JSVAL(self->height()); return JS_TRUE;
}

static JSBool setHeight(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	HTMLImageElement* self = reinterpret_cast<HTMLImageElement*>(JS_GetPrivate(cx, obj));
	self->_height = JSVAL_TO_INT(vp); return JS_TRUE;
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

static JSPropertySpec properties[] = {
	{"src", 0, 0, getSrc, setSrc},
	{"width", 0, 0, getWidth, setWidth},
	{"height", 0, 0, getHeight, setHeight},
	{"naturalWidth", 0, 0, naturalWidth, JS_PropertyStub},
	{"naturalHeight", 0, 0, naturalHeight, JS_PropertyStub},
	{"complete", 0, 0, complete, JS_PropertyStub},
	{0}
};

static JSBool construct(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	if(!JS_IsConstructing(cx)) return JS_FALSE;	// Not called as constructor? (called without new)

	HTMLImageElement* img = new HTMLImageElement;
	img->bind(cx, NULL);

	Rhinoca* rh = reinterpret_cast<Rhinoca*>(JS_GetContextPrivate(cx));
	img->ownerDocument = rh->domWindow->document;

	*rval = OBJECT_TO_JSVAL(img->jsObject);

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
	jsObject = JS_NewObject(cx, &jsClass, Element::createPrototype(), NULL);
	VERIFY(JS_SetPrivate(cx, jsObject, this));
	VERIFY(JS_DefineFunctions(cx, jsObject, methods));
	VERIFY(JS_DefineProperties(cx, jsObject, properties));
	addReference();
}

void HTMLImageElement::registerClass(JSContext* cx, JSObject* parent)
{
	JS_InitClass(cx, parent, NULL, &jsClass, &construct, 0, NULL, NULL, NULL, NULL);
}

Element* HTMLImageElement::factoryCreate(Rhinoca* rh, const char* type, XmlParser* parser)
{
	return strcasecmp(type, "IMG") == 0 ? new HTMLImageElement : NULL;
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

const char* HTMLImageElement::tagName() const
{
	return "IMG";
}

}	// namespace Dom
