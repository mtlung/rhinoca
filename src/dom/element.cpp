#include "pch.h"
#include "element.h"
#include "document.h"
#include "nodelist.h"
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
	JS_PropertyStub,  JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub,  JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSBool clientWidth(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	Element* self = getJsBindable<Element>(cx, obj);
	*vp = INT_TO_JSVAL(self->clientWidth());
	return JS_TRUE;
}

static JSBool clientHeight(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	Element* self = getJsBindable<Element>(cx, obj);
	*vp = INT_TO_JSVAL(self->clientHeight());
	return JS_TRUE;
}

static JSBool elementGetStyle(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	Element* self = getJsBindable<Element>(cx, obj);

	ElementStyle* style = new ElementStyle;
	style->element = self;
	style->bind(cx, *self);
	*vp = *style;

	return JS_TRUE;
}

static JSBool tagName(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	Element* self = getJsBindable<Element>(cx, obj);
	*vp = STRING_TO_JSVAL(JS_InternString(cx, self->tagName()));
	return JS_TRUE;
}

static const char* _eventAttributeTable[] = {
	"onmouseup",
	"onmousedown",
	"onmousemove"
};

static JSBool getEventAttribute(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	// Not implemented
	return JS_FALSE;
}

static JSBool setEventAttribute(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	Element* self = getJsBindable<Element>(cx, obj);
	int32 idx = JSID_TO_INT(id) / 2 + 0;	// Account for having both get and set functions

	return self->addEventListenerAsAttribute(cx, _eventAttributeTable[idx], *vp);
}

static JSPropertySpec elementProperties[] = {
	{"clientWidth", 0, JSPROP_READONLY | JsBindable::jsPropFlags, clientWidth, JS_StrictPropertyStub},
	{"clientHeight", 0, JSPROP_READONLY | JsBindable::jsPropFlags, clientHeight, JS_StrictPropertyStub},
	{"style", 0, JSPROP_READONLY | JsBindable::jsPropFlags, elementGetStyle, JS_StrictPropertyStub},
	{"tagName", 0, JSPROP_READONLY | JsBindable::jsPropFlags, tagName, JS_StrictPropertyStub},

	// Event attributes
	{_eventAttributeTable[0], 0, 0, getEventAttribute, setEventAttribute},
	{_eventAttributeTable[1], 1, 0, getEventAttribute, setEventAttribute},
	{_eventAttributeTable[2], 2, 0, getEventAttribute, setEventAttribute},
	{0}
};

static JSBool getAttribute(JSContext* cx, uintN argc, jsval* vp)
{
	Element* self = getJsBindable<Element>(cx, vp);
	if(!self) return JS_FALSE;

	JsString jss(cx, JS_ARGV0);
	if(!jss) return JS_FALSE;

	// NOTE: Simply returning JS_GetProperty() will give an 'undefined' value, but the
	// DOM specification says that the correct return should be empty string.
	// However most browser returns a null and so we follows.
	// See: https://developer.mozilla.org/en/DOM/element.getAttribute#Notes
	JSBool found;
	tolower(jss.c_str());
	if(!JS_HasProperty(cx, *self, jss.c_str(), &found)) return JS_FALSE;

	if(!found) {
		JS_RVAL(cx, vp) = JSVAL_NULL;
		return JS_TRUE;
	}

	return JS_GetProperty(cx, *self, jss.c_str(), vp);
}

static JSBool hasAttribute(JSContext* cx, uintN argc, jsval* vp)
{
	Element* self = getJsBindable<Element>(cx, vp);
	if(!self) return JS_FALSE;

	JsString jss(cx, JS_ARGV0);
	if(!jss) return JS_FALSE;

	JSBool found;
	tolower(jss.c_str());
	if(!JS_HasProperty(cx, *self, jss.c_str(), &found)) return JS_FALSE;

	JS_RVAL(cx, vp) = BOOLEAN_TO_JSVAL(found);
	return JS_TRUE;
}

static JSBool setAttribute(JSContext* cx, uintN argc, jsval* vp)
{
	Element* self = getJsBindable<Element>(cx, vp);
	if(!self) return JS_FALSE;

	JsString jss(cx, JS_ARGV0);
	if(!jss) return JS_FALSE;

	return JS_SetProperty(cx, *self, jss.c_str(), &JS_ARGV1);
}

static JSBool getElementsByTagName(JSContext* cx, uintN argc, jsval* vp)
{
	Element* self = getJsBindable<Element>(cx, vp);
	if(!self) return JS_FALSE;

	JsString jss(cx, JS_ARGV0);
	if(!jss) return JS_FALSE;

	toupper(jss.c_str());
	JS_RVAL(cx, vp) = *self->getElementsByTagName(jss.c_str());

	return JS_TRUE;
}

static JSFunctionSpec elementMethods[] = {
	{"getAttribute", getAttribute, 1,0},
	{"hasAttribute", hasAttribute, 1,0},
	{"setAttribute", setAttribute, 2,0},
	{"getElementsByTagName", getElementsByTagName, 1,0},
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
	VERIFY(JS_InitClass(cx, parent, NULL, &jsClass, NULL, 0, NULL, NULL, NULL, NULL));
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
		// Relative path to the document, convert it to absolute path
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
	JS_PropertyStub,  JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub,  JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSBool styleGetVisible(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	ElementStyle* self = getJsBindable<ElementStyle>(cx, obj);
	*vp = BOOLEAN_TO_JSVAL(self->element->visible);
	return JS_TRUE;
}

static JSBool styleSetVisible(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	JSBool b;
	ElementStyle* self = getJsBindable<ElementStyle>(cx, obj);
	if(!JS_ValueToBoolean(cx, *vp, &b)) return JS_FALSE;
	self->element->visible = (b ==  JS_TRUE);
	return JS_TRUE;
}

static JSBool styleGetTop(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	ElementStyle* self = getJsBindable<ElementStyle>(cx, obj);
	*vp = INT_TO_JSVAL(self->element->top);
	return JS_TRUE;
}

static JSBool styleSetTop(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	ElementStyle* self = getJsBindable<ElementStyle>(cx, obj);
	return JS_ValueToRhInt32(cx, *vp, &self->element->top);
}

static JSBool styleGetLeft(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	ElementStyle* self = getJsBindable<ElementStyle>(cx, obj);
	*vp = INT_TO_JSVAL(self->element->left);
	return JS_TRUE;
}

static JSBool styleSetLeft(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	ElementStyle* self = getJsBindable<ElementStyle>(cx, obj);
	return JS_ValueToRhInt32(cx, *vp, &self->element->left);
}

static JSPropertySpec properties[] = {
	{"display", 0, JsBindable::jsPropFlags, styleGetVisible, styleSetVisible},
	{"top", 0, JsBindable::jsPropFlags, styleGetTop, styleSetTop},
	{"left", 0, JsBindable::jsPropFlags, styleGetLeft, styleSetLeft},
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
