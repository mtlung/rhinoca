#include "pch.h"
#include "elementstyle.h"
#include "element.h"
#include "cssparser.h"
#include "document.h"
#include "../context.h"
#include "../path.h"

using namespace Parsing;

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

static JSBool styleSetLeft(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	ElementStyle* self = getJsBindable<ElementStyle>(cx, obj);
	float val;
	if(!JS_ValueToCssDimension(cx, *vp, val))
		return JS_FALSE;
	self->element->setLeft(val);
	return JS_TRUE;
}

static JSBool styleSetRight(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	ElementStyle* self = getJsBindable<ElementStyle>(cx, obj);
	float val;
	if(!JS_ValueToCssDimension(cx, *vp, val))
		return JS_FALSE;
	self->element->setRight(val);
	return JS_TRUE;
}

static JSBool styleSetTop(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	ElementStyle* self = getJsBindable<ElementStyle>(cx, obj);
	float val;
	if(!JS_ValueToCssDimension(cx, *vp, val))
		return JS_FALSE;
	self->element->setTop(val);
	return JS_TRUE;
}

static JSBool styleSetBottom(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	ElementStyle* self = getJsBindable<ElementStyle>(cx, obj);
	float val;
	if(!JS_ValueToCssDimension(cx, *vp, val))
		return JS_FALSE;
	self->element->setBottom(val);
	return JS_TRUE;
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

	JsString jss(cx, *vp);
	if(!jss || !self->setBackgroundImage(jss.c_str()))
		return JS_FALSE;

	return JS_TRUE;
}

static JSBool styleSetBGColor(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	ElementStyle* self = getJsBindable<ElementStyle>(cx, obj);

	JsString jss(cx, *vp);
	if(!jss || !self->setBackgroundColor(jss.c_str()))
		return JS_FALSE;

	return JS_TRUE;
}

static JSBool styleSetBGPos(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	ElementStyle* self = getJsBindable<ElementStyle>(cx, obj);

	JsString jss(cx, *vp);
	if(!jss || !self->setBackgroundImage(jss.c_str()))
		return JS_FALSE;

	return JS_TRUE;
}

static JSPropertySpec properties[] = {
	{"display", 0, JsBindable::jsPropFlags, JS_PropertyStub, styleSetVisible},
	{"left", 0, JsBindable::jsPropFlags, JS_PropertyStub, styleSetLeft},
	{"right", 0, JsBindable::jsPropFlags, JS_PropertyStub, styleSetRight},
	{"top", 0, JsBindable::jsPropFlags, JS_PropertyStub, styleSetTop},
	{"bottom", 0, JsBindable::jsPropFlags, JS_PropertyStub, styleSetBottom},
	{"width", 0, JsBindable::jsPropFlags, JS_PropertyStub, styleSetWidth},
	{"height", 0, JsBindable::jsPropFlags, JS_PropertyStub, styleSetHeight},
	{"backgroundImage", 0, JsBindable::jsPropFlags, JS_PropertyStub, styleSetBG},
	{"backgroundColor", 0, JsBindable::jsPropFlags, JS_PropertyStub, styleSetBGColor},
	{"backgroundPosition", 0, JsBindable::jsPropFlags, JS_PropertyStub, styleSetBGPos},
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
	, backgroundColor(0, 0, 0, 0)
{
}

ElementStyle::~ElementStyle()
{
}

struct ParserState
{
	ElementStyle* style;
	const char* propNameBegin, *propNameEnd;
};

static void parserCallback(ParserResult* result, Parser* parser)
{
	ParserState* state = reinterpret_cast<ParserState*>(parser->userdata);
	ElementStyle* style = state->style;

	if(strcmp(result->type, "propName") == 0) {
		state->propNameBegin = result->begin;
		state->propNameEnd = result->end;
	}
	else if(strcmp(result->type, "propVal") == 0) {
		char nameEnd = *state->propNameEnd;
		char valueEnd = *result->end;
		*const_cast<char*>(state->propNameEnd) = '\0';
		*const_cast<char*>(result->end) = '\0';
		style->setStyleAttribute(state->propNameBegin, result->begin);
		*const_cast<char*>(state->propNameEnd) = nameEnd;
		*const_cast<char*>(result->end) = valueEnd;
	}
}

void ElementStyle::setStyleString(const char* begin, const char* end)
{
	ParserState state = { this, NULL, NULL };
	Parser parser(begin, end, parserCallback, &state);
	Parsing::declarations(&parser, false).once();
}

void ElementStyle::setStyleAttribute(const char* name, const char* value)
{
	StringLowerCaseHash hash(name, 0);

	if(hash == StringHash("left")) {
		float v = 0;
		sscanf(value, "%f", &v);
		setLeft(v);
	}
	if(hash == StringHash("right")) {
		float v = 0;
		sscanf(value, "%f", &v);
		setRight(v);
	}
	else if(hash == StringHash("top")) {
		float v = 0;
		sscanf(value, "%f", &v);
		setTop(v);
	}
	else if(hash == StringHash("bottom")) {
		float v = 0;
		sscanf(value, "%f", &v);
		setBottom(v);
	}
	else if(hash == StringHash("width")) {
		float v = 0;
		sscanf(value, "%f", &v);
		setWidth((unsigned)v);
	}
	else if(hash == StringHash("height")) {
		float v = 0;
		sscanf(value, "%f", &v);
		setHeight((unsigned)v);
	}
	else if(hash == StringHash("background-image")) {
		setBackgroundImage(value);
	}
	else if(hash == StringHash("backgroundcolor")) {
		setBackgroundColor(value);
	}
	else if(hash == StringHash("background-position")) {
		// For sscanf formatting: http://linux.die.net/man/3/scanf
		sscanf(value, "%f%*[ ,\n\r\t]%f", &backgroundPositionX, &backgroundPositionY);
	}
}

bool ElementStyle::visible() const { return element->visible; }
float ElementStyle::left() const { return element->left(); }
float ElementStyle::right() const { return element->right(); }
float ElementStyle::top() const { return element->top(); }
float ElementStyle::bottom() const { return element->bottom(); }
unsigned ElementStyle::width() const { return element->width(); }
unsigned ElementStyle::height() const { return element->height(); }

void ElementStyle::setVisible(bool val) { element->visible = val; }
void ElementStyle::setLeft(float val) { element->setLeft(val); }
void ElementStyle::setRight(float val) { element->setRight(val); }
void ElementStyle::setTop(float val) { element->setTop(val); }
void ElementStyle::setBottom(float val) { element->setBottom(val); }
void ElementStyle::setWidth(unsigned val) { element->setWidth(val); }
void ElementStyle::setHeight(unsigned val) { element->setHeight(val); }

bool ElementStyle::setBackgroundPosition(const char* cssBackgroundPosition)
{
	return sscanf(cssBackgroundPosition, "%f%*[ ,\n\r\t]%f", &backgroundPositionX, &backgroundPositionY) == 2;
}

bool ElementStyle::setBackgroundImage(const char* cssUrl)
{
	// Get the url from the style string value
	Parsing::Parser parser(cssUrl, cssUrl + strlen(cssUrl));
	Parsing::ParserResult result;

	if(!Parsing::url(&parser).once(&result)) {
		// TODO: Give some warning
		return false;
	}

	char bk = *result.end;
	*const_cast<char*>(result.end) = '\0';	// Make it parse result null terminated

	Path path;
	element->fixRelativePath(result.begin, element->rhinoca->documentUrl.c_str(), path);

	*const_cast<char*>(result.end) = bk;

	ResourceManager& mgr = element->rhinoca->resourceManager;
	backgroundImage = mgr.loadAs<Render::Texture>(path.c_str());

	return true;
}

bool ElementStyle::setBackgroundColor(const char* cssColor)
{
	return backgroundColor.parse(cssColor);
}

}	// namespace Dom
