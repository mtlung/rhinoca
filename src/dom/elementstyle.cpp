#include "pch.h"
#include "elementstyle.h"
#include "element.h"
#include "cssparser.h"
#include "../context.h"
#include "../path.h"

namespace Dom {

static JSBool JS_ValueToCssDimension(JSContext *cx, jsval v, float& val)
{
	jsdouble d = 0;
	JSBool ret;

	if(JSVAL_IS_NUMBER(v)) {
		ret = JS_ValueToNumber(cx, v, &d);
		val = (float)d;
	}
	else {
		JsString jss(cx, v);
		if(!jss) ret = JS_FALSE;
		char buf[32];
		ret = (sscanf(jss.c_str(), "%f%s", &val, buf) == 2) ? JS_TRUE : JS_FALSE;

		if(strcasecmp(buf, "px") != 0) {
			Rhinoca* rh = reinterpret_cast<Rhinoca*>(JS_GetContextPrivate(cx));
			print(rh, "%s", "Only 'px' is supported for CSS dimension unit");
		}
	}

	return ret;
}

JSClass ElementStyle::jsClass = {
	"ElementStyle", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub,  JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub,  JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSBool styleSetVisible(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	JSBool b;
	ElementStyle* self = getJsBindable<ElementStyle>(cx, obj);
	if(!JS_ValueToBoolean(cx, *vp, &b)) return JS_FALSE;
	self->element->visible = (b ==  JS_TRUE);
	return JS_TRUE;
}

static JSBool styleSetTop(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	ElementStyle* self = getJsBindable<ElementStyle>(cx, obj);
	return JS_ValueToCssDimension(cx, *vp, self->element->_top);
}

static JSBool styleSetLeft(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	ElementStyle* self = getJsBindable<ElementStyle>(cx, obj);
	return JS_ValueToCssDimension(cx, *vp, self->element->_left);
}

static JSBool styleSetWidth(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	ElementStyle* self = getJsBindable<ElementStyle>(cx, obj);

	float w = 0;
	if(!JS_ValueToCssDimension(cx, *vp, w))
		return JS_FALSE;

	self->setWidth((unsigned)w);
	return JS_TRUE;
}

static JSBool styleSetHeight(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	ElementStyle* self = getJsBindable<ElementStyle>(cx, obj);

	float h = 0;
	if(!JS_ValueToCssDimension(cx, *vp, h))
		return JS_FALSE;

	self->setHeight((unsigned)h);
	return JS_TRUE;
}

static JSBool styleSetBG(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	ElementStyle* self = getJsBindable<ElementStyle>(cx, obj);
	Element* ele = self->element;

	JsString jss(cx, *vp);
	if(!jss) return JS_FALSE;

	// Get the url from the style string value
	Parsing::Parser parser(jss.c_str(), jss.c_str() + jss.size());
	Parsing::ParserResult result;

	if(!Parsing::url(&parser).once(&result)) {
		// TODO: Give some warning
		return JS_TRUE;
	}
	*result.end = '\0';	// Make it parse result null terminated

	Path path;
	ele->fixRelativePath(result.begin, ele->rhinoca->documentUrl.c_str(), path);

	using namespace Render;
	
	ResourceManager& mgr = ele->rhinoca->resourceManager;
	self->backgroundImage = mgr.loadAs<Texture>(path.c_str());

	return JS_TRUE;
}

static JSBool styleSetBGPos(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	ElementStyle* self = getJsBindable<ElementStyle>(cx, obj);

	if(!JS_ValueToCssDimension(cx, *vp, self->backgroundPositionX))
		return JS_FALSE;

	return JS_TRUE;
}

static JSPropertySpec properties[] = {
	{"display", 0, JsBindable::jsPropFlags, JS_PropertyStub, styleSetVisible},
	{"top", 0, JsBindable::jsPropFlags, JS_PropertyStub, styleSetTop},
	{"left", 0, JsBindable::jsPropFlags, JS_PropertyStub, styleSetLeft},
	{"width", 0, JsBindable::jsPropFlags, JS_PropertyStub, styleSetWidth},
	{"height", 0, JsBindable::jsPropFlags, JS_PropertyStub, styleSetHeight},
	{"backgroundImage", 0, JsBindable::jsPropFlags, JS_PropertyStub, styleSetBG},
	{"backgroundPositionX", 0, JsBindable::jsPropFlags, JS_PropertyStub, styleSetBGPos},
	{0}
};

void ElementStyle::bind(JSContext* cx, JSObject* parent)
{
	ASSERT(!jsContext);
	jsContext = cx;
	jsObject = JS_NewObject(cx, &jsClass, NULL, parent);
	VERIFY(JS_SetPrivate(cx, *this, this));
	VERIFY(JS_DefineProperties(cx, *this, properties));
	addReference();	// releaseReference() in JsBindable::finalize()
}

ElementStyle::ElementStyle(Element* ele)
	: element(ele)
	, backgroundPositionX(0), backgroundPositionY(0)
{
}

ElementStyle::~ElementStyle()
{
}

bool ElementStyle::visible() const { return element->visible; }
float ElementStyle::left() const { return element->left(); }
float ElementStyle::top() const { return element->top(); }
unsigned ElementStyle::width() const { return element->width(); }
unsigned ElementStyle::height() const { return element->height(); }

void ElementStyle::setVisible(bool val) { element->visible = val; }
void ElementStyle::setLeft(float val) { element->_left = val; }
void ElementStyle::setTop(float val) { element->_top = val; }
void ElementStyle::setWidth(unsigned val) { element->setWidth(val); }
void ElementStyle::setHeight(unsigned val) { element->setheight(val); }

}	// namespace Dom
