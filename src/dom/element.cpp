#include "pch.h"
#include "element.h"
#include "document.h"
#include "nodelist.h"
#include "../array.h"
#include "../context.h"
#include "../path.h"

static JSBool JS_ValueToRhInt32(JSContext *cx, jsval v, rhint32 *ip)
{
	int32 val = *ip;
	JSBool ret = JS_ValueToInt32(cx, v, &val);
	*ip = val;
	return ret;
}

namespace Dom {

JSClass Element::jsClass = {
	"Element", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub,  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub,  JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSBool clientWidth(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	Element* self = reinterpret_cast<Element*>(JS_GetPrivate(cx, obj));
	*vp = INT_TO_JSVAL(self->clientWidth());
	return JS_TRUE;
}

static JSBool clientHeight(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	Element* self = reinterpret_cast<Element*>(JS_GetPrivate(cx, obj));
	*vp = INT_TO_JSVAL(self->clientHeight());
	return JS_TRUE;
}

static JSBool elementGetStyle(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	Element* self = reinterpret_cast<Element*>(JS_GetPrivate(cx, obj));

	ElementStyle* style = new ElementStyle;
	style->element = self;
	style->bind(cx, *self);
	*vp = *style;

	return JS_TRUE;
}

static JSBool tagName(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	Element* self = reinterpret_cast<Element*>(JS_GetPrivate(cx, obj));
	*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, self->tagName()));
	return JS_TRUE;
}

const Array<const char*, 3> _eventAttributeTable = {
	"onmouseup",
	"onmousedown",
	"onmousemove"
};

static JSBool getEventAttribute(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	// Not implemented
	return JS_FALSE;
}

static JSBool setEventAttribute(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	Node* self = reinterpret_cast<Node*>(JS_GetPrivate(cx, obj));
	id /= 2 + 0;	// Account for having both get and set functions

	return self->addEventListenerAsAttribute(cx, _eventAttributeTable[id], *vp);
}

static JSPropertySpec elementProperties[] = {
	{"clientWidth", 0, JSPROP_READONLY, clientWidth, JS_PropertyStub},
	{"clientHeight", 0, JSPROP_READONLY, clientHeight, JS_PropertyStub},
	{"style", 0, JSPROP_READONLY, elementGetStyle, JS_PropertyStub},
	{"tagName", 0, JSPROP_READONLY, tagName, JS_PropertyStub},

	// Event attributes
	{_eventAttributeTable[0], 0, 0, getEventAttribute, setEventAttribute},
	{_eventAttributeTable[1], 1, 0, getEventAttribute, setEventAttribute},
	{_eventAttributeTable[2], 2, 0, getEventAttribute, setEventAttribute},
	{0}
};

static JSBool getAttribute(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
//	if(!JS_InstanceOf(cx, obj, &Element::jsClass, argv)) return JS_FALSE;
	Element* self = reinterpret_cast<Element*>(JS_GetPrivate(cx, obj));
	if(!self) return JS_FALSE;

	if(JSString* jss = JS_ValueToString(cx, argv[0])) {
		char* str = JS_GetStringBytes(jss);
		return JS_GetProperty(cx, *self, str, rval);
	}

	return JS_FALSE;
}

static JSBool setAttribute(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
//	if(!JS_InstanceOf(cx, obj, &Element::jsClass, argv)) return JS_FALSE;
	Element* self = reinterpret_cast<Element*>(JS_GetPrivate(cx, obj));
	if(!self) return JS_FALSE;

	if(JSString* jss = JS_ValueToString(cx, argv[0])) {
		char* str = JS_GetStringBytes(jss);
		return JS_SetProperty(cx, *self, str, &argv[1]);
	}

	return JS_FALSE;
}

static JSBool getElementsByTagName(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	Element* self = reinterpret_cast<Element*>(JS_GetPrivate(cx, obj));
	JSString* jss = JS_ValueToString(cx, argv[0]);
	char* str = JS_GetStringBytes(jss);
	toupper(str);
	*rval = *self->getElementsByTagName(str);

	return JS_TRUE;
}

static JSFunctionSpec elementMethods[] = {
	{"getAttribute", getAttribute, 1,0,0},
	{"setAttribute", setAttribute, 2,0,0},
	{"getElementsByTagName", getElementsByTagName, 1,0,0},
	{0}
};

Element::Element()
	: visible(true)
	, top(0), left(0)
{
}

JSObject* Element::createPrototype()
{
	ASSERT(jsContext);
	JSObject* proto = JS_NewObject(jsContext, &jsClass, Node::createPrototype(), NULL);
	VERIFY(JS_SetPrivate(jsContext, proto, this));
	VERIFY(JS_DefineFunctions(jsContext, proto, elementMethods));
	VERIFY(JS_DefineProperties(jsContext, proto, elementProperties));
	addReference();
	return proto;
}

void Element::registerClass(JSContext* cx, JSObject* parent)
{
	JS_InitClass(cx, parent, NULL, &jsClass, NULL, 0, NULL, NULL, NULL, NULL);
}

static Node* getElementsByTagNameFilter_(NodeIterator& iter, void* userData)
{
	FixString s((rhuint32)userData);
	Element* ele = dynamic_cast<Element*>(iter.current());
	iter.next();

	if(ele && ele->tagName() == s)
		return ele;

	return NULL;
}

NodeList* Element::getElementsByTagName(const char* tagName)
{
	NodeList* list = new NodeList(this, getElementsByTagNameFilter_, (void*)StringHash(tagName, 0).hash);
	list->bind(jsContext, NULL);
	return list;
}

void Element::fixRelativePath(const char* uri, const char* docUri, Path& path)
{
	if(Path(uri).hasRootDirectory())	// Absolute path
		path = uri;
	else {
		// Relative path to the document
		path = docUri;
		path = path.getBranchPath() / uri;
	}
}

static const FixString _tagName = "ELEMENT";

const FixString& Element::tagName() const
{
	return _tagName;
}

ElementFactory::ElementFactory()
	: _factories(NULL)
	, _factoryCount(0)
	, _factoryBufCount(0)
{
}

ElementFactory::~ElementFactory()
{
	rhdelete(_factories);
}

ElementFactory& ElementFactory::singleton()
{
	static ElementFactory factory;
	return factory;
}

Element* ElementFactory::create(Rhinoca* rh, const char* type, XmlParser* parser)
{
	for(int i=0; i<_factoryCount; ++i) {
		if(Element* e = _factories[i](rh, type, parser))
			return e;
	}
	return NULL;
}

void ElementFactory::addFactory(FactoryFunc factory)
{
	++_factoryCount;

	if(_factoryBufCount < _factoryCount) {
		int oldBufCount = _factoryBufCount;
		_factoryBufCount = (_factoryBufCount == 0) ? 2 : _factoryBufCount * 2;
		_factories = rhrenew(_factories, oldBufCount, _factoryBufCount);
	}

	_factories[_factoryCount-1] = factory;
}

JSClass ElementStyle::jsClass = {
	"ElementStyle", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub,  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub,  JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSBool styleGetVisible(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	ElementStyle* self = reinterpret_cast<ElementStyle*>(JS_GetPrivate(cx, obj));
	*vp = BOOLEAN_TO_JSVAL(self->element->visible);
	return JS_TRUE;
}

static JSBool styleSetVisible(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	JSBool b;
	ElementStyle* self = reinterpret_cast<ElementStyle*>(JS_GetPrivate(cx, obj));
	if(!JS_ValueToBoolean(cx, *vp, &b)) return JS_FALSE;
	self->element->visible = (b ==  JS_TRUE);
	return JS_TRUE;
}

static JSBool styleGetTop(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	ElementStyle* self = reinterpret_cast<ElementStyle*>(JS_GetPrivate(cx, obj));
	*vp = INT_TO_JSVAL(self->element->top);
	return JS_TRUE;
}

static JSBool styleSetTop(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	ElementStyle* self = reinterpret_cast<ElementStyle*>(JS_GetPrivate(cx, obj));
	return JS_ValueToRhInt32(cx, *vp, &self->element->top);
}

static JSBool styleGetLeft(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	ElementStyle* self = reinterpret_cast<ElementStyle*>(JS_GetPrivate(cx, obj));
	*vp = INT_TO_JSVAL(self->element->left);
	return JS_TRUE;
}

static JSBool styleSetLeft(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	ElementStyle* self = reinterpret_cast<ElementStyle*>(JS_GetPrivate(cx, obj));
	return JS_ValueToRhInt32(cx, *vp, &self->element->left);
}

static JSPropertySpec properties[] = {
	{"display", 0, 0, styleGetVisible, styleSetVisible},
	{"top", 0, 0, styleGetTop, styleSetTop},
	{"left", 0, 0, styleGetLeft, styleSetLeft},
	{0}
};

void ElementStyle::bind(JSContext* cx, JSObject* parent)
{
	ASSERT(!jsContext);
	jsContext = cx;
	jsObject = JS_NewObject(cx, &jsClass, NULL, parent);
	VERIFY(JS_SetPrivate(cx, *this, this));
	VERIFY(JS_DefineProperties(cx, *this, properties));
	addReference();
}

}	// namespace Dom
