#include "pch.h"
#include "element.h"

namespace Dom {

JSClass Element::jsClass = {
	"Element", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub,  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub,  JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSBool elementGetStyle(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	Element* self = reinterpret_cast<Element*>(JS_GetPrivate(cx, obj));

	ElementStyle* style = new ElementStyle;
	style->element = self;
	style->bind(cx, self->jsObject);
	*vp = OBJECT_TO_JSVAL(style->jsObject);

	return JS_TRUE;
}

static JSPropertySpec elementProperties[] = {
	{"style", 0, 0, elementGetStyle, JS_PropertyStub},
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
//	VERIFY(JS_DefineFunctions(jsContext, proto, methods));
	VERIFY(JS_DefineProperties(jsContext, proto, elementProperties));
	addReference();
	return proto;
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
	return JS_ValueToInt32(cx, *vp, &self->element->top);
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
	return JS_ValueToInt32(cx, *vp, &self->element->left);
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
	jsObject = JS_NewObject(cx, &jsClass, NULL, NULL);
	VERIFY(JS_SetPrivate(cx, jsObject, this));
	VERIFY(JS_DefineProperties(cx, jsObject, properties));
	addReference();
}

}	// namespace Dom
