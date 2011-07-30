#include "pch.h"
#include "element.h"
#include "elementstyle.h"
#include "canvas2dcontext.h"
#include "document.h"
#include "nodelist.h"
#include "../context.h"
#include "../path.h"

namespace Dom {

JSClass Element::jsClass = {
	"HTMLElement", JSCLASS_HAS_PRIVATE | JSCLASS_HAS_RESERVED_SLOTS(1),
	JS_PropertyStub,  JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub,  JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSBool clientWidth(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	Element* self = getJsBindable<Element>(cx, obj);
	*vp = INT_TO_JSVAL(self->width());
	return JS_TRUE;
}

static JSBool clientHeight(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	Element* self = getJsBindable<Element>(cx, obj);
	*vp = INT_TO_JSVAL(self->height());
	return JS_TRUE;
}

static JSBool elementGetStyle(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	Element* self = getJsBindable<Element>(cx, obj);

	if(!self->style) {
		self->style = new ElementStyle(self);
		self->style->bind(cx, *self);
		VERIFY(JS_SetReservedSlot(cx, self->jsObjectOfType(&Element::jsClass), 0, *self->style));
	}

	*vp = *self->style;

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
	"onmousemove",
	"onkeyup",
	"onkeydown",
};

static JSBool getEventAttribute(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	Element* self = getJsBindable<Element>(cx, obj);
	int32 idx = JSID_TO_INT(id);

	*vp = self->getEventListenerAsAttribute(cx, _eventAttributeTable[idx]);
	return JS_TRUE;
}

static JSBool setEventAttribute(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	Element* self = getJsBindable<Element>(cx, obj);
	int32 idx = JSID_TO_INT(id);

	return self->addEventListenerAsAttribute(cx, _eventAttributeTable[idx], *vp);
}

static JSPropertySpec elementProperties[] = {
	{"clientWidth", 0, JSPROP_READONLY | JsBindable::jsPropFlags, clientWidth, JS_StrictPropertyStub},
	{"clientHeight", 0, JSPROP_READONLY | JsBindable::jsPropFlags, clientHeight, JS_StrictPropertyStub},
	{"style", 0, JSPROP_READONLY | JsBindable::jsPropFlags, elementGetStyle, JS_StrictPropertyStub},
	{"tagName", 0, JSPROP_READONLY | JsBindable::jsPropFlags, tagName, JS_StrictPropertyStub},

	// Event attributes
	{_eventAttributeTable[0], 0, JsBindable::jsPropFlags, getEventAttribute, setEventAttribute},
	{_eventAttributeTable[1], 1, JsBindable::jsPropFlags, getEventAttribute, setEventAttribute},
	{_eventAttributeTable[2], 2, JsBindable::jsPropFlags, getEventAttribute, setEventAttribute},
	{_eventAttributeTable[3], 3, JsBindable::jsPropFlags, getEventAttribute, setEventAttribute},
	{_eventAttributeTable[4], 4, JsBindable::jsPropFlags, getEventAttribute, setEventAttribute},
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
		*vp = JSVAL_NULL;
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

	*vp = BOOLEAN_TO_JSVAL(found);
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

Element::Element(Rhinoca* rh)
	: Node(rh)
	, _top(0), _left(0)
	, _width(0), _height(0)
	, style(NULL)
{
}

JSObject* Element::createPrototype()
{
	ASSERT(jsContext);
	JSObject* proto = JS_NewObject(jsContext, &jsClass, Node::createPrototype(), NULL);
	VERIFY(JS_SetPrivate(jsContext, proto, this));
	VERIFY(JS_DefineFunctions(jsContext, proto, elementMethods));
	VERIFY(JS_DefineProperties(jsContext, proto, elementProperties));
	addReference();	// releaseReference() in JsBindable::finalize()
	return proto;
}

static JSBool construct(JSContext* cx, uintN argc, jsval* vp)
{
	ASSERT(false && "For compatible with javascript instanceof operator only, you are not suppose to new a HTMLElement directly");
	return JS_FALSE;
}

void Element::registerClass(JSContext* cx, JSObject* parent)
{
	VERIFY(JS_InitClass(cx, parent, NULL, &jsClass, construct, 0, NULL, NULL, NULL, NULL));
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

static int _min(int a, int b) { return a < b ? a : b; }

void Element::render()
{
	// Render the style's background image if any
	if(!style || !style->backgroundImage)
		return;

	HTMLCanvasElement* canvas = ownerDocument()->window()->virtualCanvas;
	CanvasRenderingContext2D* context = dynamic_cast<CanvasRenderingContext2D*>(canvas->context);

	float dx = (float)style->left();
	float dy = (float)style->top();
	float dw = (float)style->width();
	float dh = (float)style->height();

	float sx = (float)-style->backgroundPositionX;
	float sy = (float)-style->backgroundPositionY;
	float sw = (float)_min(style->width(), style->backgroundImage->virtualWidth);
	float sh = (float)_min(style->height(), style->backgroundImage->virtualHeight);

	context->drawImage(
		style->backgroundImage.get(), Render::Driver::SamplerState::MIN_MAG_LINEAR,
		sx, sy, sw, sh,
		dx, dy, dw, dh
	);
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

}	// namespace Dom
