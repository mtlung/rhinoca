#include "pch.h"
#include "image.h"
#include "document.h"
#include "../context.h"
#include "../path.h"
#include "../resource.h"
#include "../render/texture.h"

using namespace Render;

namespace Dom {

JSClass Image::jsClass = {
	"Image", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub,  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub,  JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
};

JSBool setSrc(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	Image* self = reinterpret_cast<Image*>(JS_GetPrivate(cx, obj));

	JSString* jss = JS_ValueToString(cx, *vp);
	if(!jss) return JS_FALSE;
	char* str = JS_GetStringBytes(jss);

	// Assume relative path to the document
	Path path = self->ownerDocument->rhinoca->documentUrl;
	path = path.getBranchPath();
	path /= str;

	ResourceManager& mgr = self->ownerDocument->rhinoca->resourceManager;
	self->texture = mgr.loadAs<Texture>(path.c_str());

	return JS_TRUE;
}

static JSPropertySpec properties[] = {
	{"src", 0, 0, JS_PropertyStub, setSrc},
	{0}
};

static JSBool construct(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	if(!JS_IsConstructing(cx)) return JS_FALSE;	// Not called as constructor? (called without new)

	Image* img = new Image;
	img->bind(cx, NULL);

	Rhinoca* rh = reinterpret_cast<Rhinoca*>(JS_GetContextPrivate(cx));
	img->ownerDocument = rh->domWindow->document;

	*rval = OBJECT_TO_JSVAL(img->jsObject);

	return JS_TRUE;
}

static JSFunctionSpec methods[] = {
	{0}
};

Image::Image()
	: texture(NULL)
{
}

Image::~Image()
{
}

void Image::bind(JSContext* cx, JSObject* parent)
{
	ASSERT(!jsContext);
	jsContext = cx;
	jsObject = JS_NewObject(cx, &jsClass, Node::createPrototype(), NULL);
	VERIFY(JS_SetPrivate(cx, jsObject, this));
	VERIFY(JS_DefineFunctions(cx, jsObject, methods));
	VERIFY(JS_DefineProperties(cx, jsObject, properties));
	addReference();
}

void Image::registerClass(JSContext* cx, JSObject* parent)
{
	JS_InitClass(cx, parent, NULL, &jsClass, &construct, 0, NULL, NULL, NULL, NULL);
}

Element* Image::factoryCreate(const char* type)
{
	return strcasecmp(type, "Image") == 0 ? new Image : NULL;
}

rhuint Image::width() const
{
	return texture ? texture->width : 0;
}

rhuint Image::height() const
{
	return texture ? texture->height : 0;
}

}	// namespace Dom
