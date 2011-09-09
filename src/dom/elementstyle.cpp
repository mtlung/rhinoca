#include "pch.h"
#include "elementstyle.h"
#include "element.h"
#include "canvas2dcontext.h"
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

		if(strcasecmp(buf, "px") != 0)
			print(cx, "%s", "Only 'px' is supported for CSS dimension unit\n");
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
	ElementStyle* self = getJsBindable<ElementStyle>(cx, obj);
	if(!JS_GetValue(cx, *vp, self->element->visible)) return JS_FALSE;
	return JS_TRUE;
}

static JSBool styleSetOpacity(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	ElementStyle* self = getJsBindable<ElementStyle>(cx, obj);
	if(!JS_GetValue(cx, *vp, self->opacity)) return JS_FALSE;
	return JS_TRUE;
}

static JSBool styleSetLeft(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	ElementStyle* self = getJsBindable<ElementStyle>(cx, obj);
	float val;
	(void)JS_ValueToCssDimension(cx, *vp, val);

	self->element->setLeft(val);
	return JS_TRUE;
}

static JSBool styleSetRight(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	ElementStyle* self = getJsBindable<ElementStyle>(cx, obj);
	float val;
	(void)JS_ValueToCssDimension(cx, *vp, val);

	self->element->setRight(val);
	return JS_TRUE;
}

static JSBool styleSetTop(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	ElementStyle* self = getJsBindable<ElementStyle>(cx, obj);
	float val;
	(void)JS_ValueToCssDimension(cx, *vp, val);

	self->element->setTop(val);
	return JS_TRUE;
}

static JSBool styleSetBottom(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	ElementStyle* self = getJsBindable<ElementStyle>(cx, obj);
	float val;
	(void)JS_ValueToCssDimension(cx, *vp, val);

	self->element->setBottom(val);
	return JS_TRUE;
}

static JSBool styleSetWidth(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	ElementStyle* self = getJsBindable<ElementStyle>(cx, obj);

	float w = 0;
	(void)JS_ValueToCssDimension(cx, *vp, w);

	self->setWidth((unsigned)w);
	return JS_TRUE;
}

static JSBool styleSetHeight(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	ElementStyle* self = getJsBindable<ElementStyle>(cx, obj);

	float h = 0;
	(void)JS_ValueToCssDimension(cx, *vp, h);

	self->setHeight((unsigned)h);
	return JS_TRUE;
}

static JSBool styleGetTransform(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	ElementStyle* self = getJsBindable<ElementStyle>(cx, obj);
	// TODO: Implementation
	*vp = INT_TO_JSVAL(1);
	return JS_TRUE;
}

static JSBool styleSetTransform(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	ElementStyle* self = getJsBindable<ElementStyle>(cx, obj);

	JsString jss(cx, *vp);
	self->setTransform(jss.c_str());

	return JS_TRUE;
}

static JSBool styleSetBG(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	ElementStyle* self = getJsBindable<ElementStyle>(cx, obj);

	JsString jss(cx, *vp);
	if(!jss || !self->setBackgroundImage(jss.c_str()))
		print(cx, "Invalid CSS background image: %s\n", jss ? jss.c_str() : "");

	return JS_TRUE;
}

static JSBool styleSetBGColor(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	ElementStyle* self = getJsBindable<ElementStyle>(cx, obj);

	JsString jss(cx, *vp);
	if(!jss || !self->setBackgroundColor(jss.c_str()))
		print(cx, "Invalid CSS color: %s\n", jss ? jss.c_str() : "");

	return JS_TRUE;
}

static JSBool styleSetBGPos(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	ElementStyle* self = getJsBindable<ElementStyle>(cx, obj);

	JsString jss(cx, *vp);
	if(!jss || !self->setBackgroundPosition(jss.c_str()))
		print(cx, "Invalid CSS backgroundPosition: %s\n", jss ? jss.c_str() : "");

	return JS_TRUE;
}

static JSPropertySpec properties[] = {
	{"display", 0, JsBindable::jsPropFlags, JS_PropertyStub, styleSetVisible},
	{"opacity", 0, JsBindable::jsPropFlags, JS_PropertyStub, styleSetOpacity},
	{"left", 0, JsBindable::jsPropFlags, JS_PropertyStub, styleSetLeft},
	{"right", 0, JsBindable::jsPropFlags, JS_PropertyStub, styleSetRight},
	{"top", 0, JsBindable::jsPropFlags, JS_PropertyStub, styleSetTop},
	{"bottom", 0, JsBindable::jsPropFlags, JS_PropertyStub, styleSetBottom},
	{"width", 0, JsBindable::jsPropFlags, JS_PropertyStub, styleSetWidth},
	{"height", 0, JsBindable::jsPropFlags, JS_PropertyStub, styleSetHeight},
	{"transform", 0, JsBindable::jsPropFlags, styleGetTransform, styleSetTransform},
	{"Transform", 0, JsBindable::jsPropFlags, styleGetTransform, styleSetTransform},
	{"-moz-transform", 0, JsBindable::jsPropFlags, styleGetTransform, styleSetTransform},
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
	, opacity(1)
	, _origin(0.5f, 0.5f, 0)
	, _localToWorld(Mat44::identity)
	, _localTransformation(Mat44::identity)
	, backgroundPositionX(0), backgroundPositionY(0)
	, backgroundColor(0, 0, 0, 0)
{
}

ElementStyle::~ElementStyle()
{
}

static int _min(int a, int b) { return a < b ? a : b; }

void ElementStyle::updateTransformation()
{
	_localToWorld = localTransformation();

	// See if any parent node contains transform
	Node* parent = static_cast<Node*>(element->parentNode);
	while(parent) {
		Element* ele = dynamic_cast<Element*>(parent);

		if(ele) if(ElementStyle* s = ele->_style) {
			s->_localToWorld.mul(localTransformation(), _localToWorld);
			break;
		}
		parent = static_cast<Node*>(parent->parentNode);
	}
}

void ElementStyle::render()
{
	// Early out optimization
	if(backgroundColor.a == 0 && !backgroundImage)
		return;

	HTMLCanvasElement* canvas = element->ownerDocument()->window()->virtualCanvas;
	CanvasRenderingContext2D* context = dynamic_cast<CanvasRenderingContext2D*>(canvas->context);

	updateTransformation();

	context->setTransform(_localToWorld.data);

	context->setGlobalAlpha(opacity);

	// Render the background color
	if(backgroundColor.a > 0) {
		context->setFillColor(&backgroundColor.r);
		context->fillRect((float)left(), (float)top(), (float)width(), (float)height());
	}

	// Render the style's background image if any
	if(backgroundImage) {
		float dx = (float)left();
		float dy = (float)top();
		float dw = (float)width();
		float dh = (float)height();

		float sx = (float)-backgroundPositionX;
		float sy = (float)-backgroundPositionY;
		float sw = (float)_min(width(), backgroundImage->virtualWidth);
		float sh = (float)_min(height(), backgroundImage->virtualHeight);

		context->drawImage(
			backgroundImage.get(), Render::Driver::SamplerState::MIN_MAG_LINEAR,
			sx, sy, sw, sh,
			dx, dy, dw, dh
		);
	}
}


void ElementStyle::setStyleString(const char* begin, const char* end)
{
	struct ParserState
	{
		ElementStyle* style;
		const char* propNameBegin, *propNameEnd;

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
	};	// ParserState

	ParserState state = { this, NULL, NULL };
	Parser parser(begin, end, ParserState::parserCallback, &state);
	Parsing::declarations(&parser, false).once();
}

void ElementStyle::setStyleAttribute(const char* name, const char* value)
{
	const StringLowerCaseHash hash(name, 0);

	if(hash == StringHash("opacity")) {
		sscanf(value, "%f", &opacity);
	}
	else if(hash == StringHash("left")) {
		float v = 0;
		sscanf(value, "%f", &v);
		setLeft(v);
	}
	else if(hash == StringHash("right")) {
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
	else if(hash == StringHash("transform")) {
		setTransform(value);
	}
	else if(hash == StringHash("-moz-transform")) {
		setTransform(value);
	}
	else if(hash == StringHash("background-image")) {
		setBackgroundImage(value);
	}
	else if(hash == StringHash("backgroundcolor")) {
		setBackgroundColor(value);
	}
	else if(hash == StringHash("background-position")) {
		setBackgroundPosition(value);
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

void ElementStyle::setIdentity()
{
	_localTransformation = Mat44::identity;
}

void ElementStyle::setTransform(const char* transformStr)
{
	setIdentity();

	struct ParserState
	{
		ElementStyle* style;
		const char* nameBegin, *nameEnd;
		int valueIdx;
		float values[6];

		// TODO: Release the restriction that width/height/ and transform-origin should be 
		// appear before transform in the css <style> tag
		void applyTransform()
		{
			const StringLowerCaseHash hash(nameBegin, nameEnd - nameBegin);

			Mat44 mat;
			if(hash == StringHash("translate"))
			{
				if(valueIdx == 1) values[1] = 0;	// If the second param is not provided, assign zero
				mat = Mat44::makeTranslation(values);
			}
			else if(hash == StringHash("translatex"))
			{
				const float val[3] = { values[0], 0, 0 };
				mat = Mat44::makeTranslation(val);
			}
			else if(hash == StringHash("translatey"))
			{
				const float val[3] = { 0, values[0], 0 };
				mat = Mat44::makeTranslation(val);
			}
			else if(hash == StringHash("scale"))
			{
				if(valueIdx == 1) values[1] = values[0];	// If the second param is not provided, use the first one
				const float val[3] = { values[0], values[1], 1 };
				mat = Mat44::makeScale(val);
			}
			else if(hash == StringHash("scalex"))
			{
				const float val[3] = { values[0], 1, 1 };
				mat = Mat44::makeScale(val);
			}
			else if(hash == StringHash("scaley"))
			{
				const float val[3] = { 1, values[0], 1 };
				mat = Mat44::makeScale(val);
			}
			else if(hash == StringHash("rotate"))
			{
				static const float zaxis[3] = { 0, 0, 1 };
				mat = Mat44::makeAxisRotation(zaxis, values[0]);
			}
			else if(hash == StringHash("skew"))
			{
				ASSERT(false && "Not implemented");
			}
			else if(hash == StringHash("skewx"))
			{
				ASSERT(false && "Not implemented");
			}
			else if(hash == StringHash("skewy"))
			{
				ASSERT(false && "Not implemented");
			}
			else if(hash == StringHash("matrix"))
			{
				ASSERT(false && "Not implemented");
			}
			else
			{
				ASSERT(false && "invalid transform function");
			}

			// Apply the transform with origin adjustment
			const float origin[3] = { style->_origin.x * style->width(), style->_origin.y * style->height(), 0 };
			const float negOrigin[3] = { -origin[0], -origin[1], 0 };

			style->_localTransformation =
				style->_localTransformation *
				Mat44::makeTranslation(origin) *
				mat *
				Mat44::makeTranslation(negOrigin);
		}

		static void callback(ParserResult* result, Parser* parser)
		{
			ParserState* state = reinterpret_cast<ParserState*>(parser->userdata);
			ElementStyle* style = state->style;

			const StringHash hash(result->type, 0);

			if(hash == StringHash("name")) {
				state->nameBegin = result->begin;
				state->nameEnd = result->end;
				state->valueIdx = 0;
				memset(state->values, 0, sizeof(state->values));
			}
			else if(hash == StringHash("value")) {
				char bk = *result->end;
				*const_cast<char*>(result->end) = '\0';
				sscanf(result->begin, "%f", &state->values[state->valueIdx]);
				*const_cast<char*>(result->end) = bk;
				++state->valueIdx;
			}
			else if(hash == StringHash("unit")) {
				// Convert deg to rad
				if(result->end - result->begin > 0 && state->valueIdx == 1 &&
					strncmp(result->begin, "deg", result->end - result->begin) == 0
				)
					state->values[state->valueIdx - 1] *= 3.1415926535897932f / 180;
			}
			else if(hash == StringHash("end")) {
				state->applyTransform();
			}
			else if(hash == StringHash("none")) {
				style->setIdentity();
			}
			else
				ASSERT(false);
		}
	};	// ParserState

	ParserState state = { this, NULL, NULL, 0, {0} };
	Parser parser(transformStr, transformStr + strlen(transformStr), ParserState::callback, &state);
	Parsing::transform(&parser).any();
}

void ElementStyle::setOrigin(const Vec3& v)
{
	_origin = v;
}

const Mat44& ElementStyle::localTransformation() const
{
	return _localTransformation;
}

Mat44 ElementStyle::worldTransformation() const
{
	// TODO: Implementation
	return localTransformation();
}

bool ElementStyle::setBackgroundPosition(const char* cssBackgroundPosition)
{
	struct ParserState
	{
		ElementStyle* style;
		int valueIdx;
		float values[2];

		static void callback(ParserResult* result, Parser* parser)
		{
			ParserState* state = reinterpret_cast<ParserState*>(parser->userdata);
			ElementStyle* style = state->style;

			const StringHash hash(result->type, 0);

			if(hash == StringHash("value")) {
				char bk = *result->end;
				*const_cast<char*>(result->end) = '\0';
				sscanf(result->begin, "%f", &state->values[state->valueIdx]);
				*const_cast<char*>(result->end) = bk;
				++state->valueIdx;
			}
			else if(hash == StringHash("unit")) {
			}
			else if(hash == StringHash("end")) {
				style->backgroundPositionX = state->values[0];
				style->backgroundPositionY = state->values[1];
			}
			else
				ASSERT(false);
		}
	};	// ParserState

	ParserState state = { this, 0, { 0.0f } };
	Parser parser(cssBackgroundPosition, cssBackgroundPosition + strlen(cssBackgroundPosition), ParserState::callback, &state);
	return Parsing::backgroundPosition(&parser).once();
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
