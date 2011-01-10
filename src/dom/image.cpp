#include "pch.h"
#include "image.h"

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
/*	MCD::Path path = gCurrentDocument->documentPath;
	path = path.getBranchPath();
	path /= str;
	image->texture = gCurrentDocument->framework->resourceManager().loadAs<Texture>(path, 1000);
	printf("%s\n", str);*/

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

	*rval = OBJECT_TO_JSVAL(img->jsObject);

	return JS_TRUE;
}

static JSFunctionSpec methods[] = {
	{0}
};

Image::Image()
	: _width(0)
	, _height(0)
{
}

Image::~Image()
{
	int a = 0;
	a = a;
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

void Image::setWidth()
{
}

void Image::setHeight()
{
}

}	// namespace Dom
