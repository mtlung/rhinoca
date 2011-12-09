#include "pch.h"
#include "elementstyle.h"
#include "element.h"
#include "canvas2dcontext.h"
#include "cssparser.h"
#include "document.h"
#include "../context.h"
#include "../rhlog.h"
#include "../path.h"

using namespace Parsing;
using namespace ro;

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

		if(roStrCaseCmp(buf, "px") != 0)
			rhLog("warn", "%s", "Only 'px' is supported for CSS dimension unit\n");
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

static JSBool styleSetTransformOrigin(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	ElementStyle* self = getJsBindable<ElementStyle>(cx, obj);

	JsString jss(cx, *vp);
	self->setTransformOrigin(jss.c_str());

	return JS_TRUE;
}

static JSBool styleSetBG(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	ElementStyle* self = getJsBindable<ElementStyle>(cx, obj);

	JsString jss(cx, *vp);
	if(!jss || !self->setBackgroundImage(jss.c_str()))
		rhLog("warn", "Invalid CSS background image: %s\n", jss ? jss.c_str() : "");

	return JS_TRUE;
}

static JSBool styleSetBGColor(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	ElementStyle* self = getJsBindable<ElementStyle>(cx, obj);

	JsString jss(cx, *vp);
	if(!jss || !self->setBackgroundColor(jss.c_str()))
		rhLog("warn", "Invalid CSS color: %s\n", jss ? jss.c_str() : "");

	return JS_TRUE;
}

static JSBool styleSetBGPos(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	ElementStyle* self = getJsBindable<ElementStyle>(cx, obj);

	JsString jss(cx, *vp);
	if(!jss || !self->setBackgroundPosition(jss.c_str()))
		rhLog("warn", "Invalid CSS backgroundPosition: %s\n", jss ? jss.c_str() : "");

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
	{"transformOrigin", 0, JsBindable::jsPropFlags, JS_PropertyStub, styleSetTransformOrigin},
	{"TransformOrigin", 0, JsBindable::jsPropFlags, JS_PropertyStub, styleSetTransformOrigin},
	{"backgroundImage", 0, JsBindable::jsPropFlags, JS_PropertyStub, styleSetBG},
	{"backgroundColor", 0, JsBindable::jsPropFlags, JS_PropertyStub, styleSetBGColor},
	{"backgroundPosition", 0, JsBindable::jsPropFlags, JS_PropertyStub, styleSetBGPos},
	{0}
};

void ElementStyle::bind(JSContext* cx, JSObject* parent)
{
	RHASSERT(!jsContext);
	jsContext = cx;
	jsObject = JS_NewObject(cx, &jsClass, NULL, parent);
	RHVERIFY(JS_SetPrivate(cx, *this, this));
	RHVERIFY(JS_DefineProperties(cx, *this, properties));
	addReference();	// releaseReference() in JsBindable::finalize()
}

ElementStyle::ElementStyle(Element* ele)
	: element(ele)
	, opacity(1)
	, _origin(50, 50, 0)
	, _localToWorld(Mat44::identity)
	, _localTransformation(Mat44::identity)
	, backgroundPositionX(0), backgroundPositionY(0)
	, backgroundColor(0, 0, 0, 0)
{
	memset(_originUsePercentage, true, sizeof(_originUsePercentage));
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

void ElementStyle::render(CanvasRenderingContext2D* ctx)
{
	updateTransformation();

	ctx->setIdentity();
	ctx->translate((float)left(), (float)top());
	ctx->transform(_localToWorld.data);

	ctx->setGlobalAlpha(opacity);

	// Render the background color
	if(backgroundColor.a > 0) {
		ctx->setFillColor(&backgroundColor.r);
		ctx->fillRect(0, 0, (float)width(), (float)height());
	}

	// Render the style's background image if any
	if(backgroundImage) {
		float dx = 0;
		float dy = 0;
		float dw = (float)width();
		float dh = (float)height();

		float sx = (float)-backgroundPositionX;
		float sy = (float)-backgroundPositionY;
		float sw = (float)_min(width(), backgroundImage->virtualWidth);
		float sh = (float)_min(height(), backgroundImage->virtualHeight);

		ctx->drawImage(
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
	const StringHash hash = stringLowerCaseHash(name, 0);

	if(hash == stringHash("opacity")) {
		sscanf(value, "%f", &opacity);
	}
	else if(hash == stringHash("left")) {
		float v = 0;
		sscanf(value, "%f", &v);
		setLeft(v);
	}
	else if(hash == stringHash("right")) {
		float v = 0;
		sscanf(value, "%f", &v);
		setRight(v);
	}
	else if(hash == stringHash("top")) {
		float v = 0;
		sscanf(value, "%f", &v);
		setTop(v);
	}
	else if(hash == stringHash("bottom")) {
		float v = 0;
		sscanf(value, "%f", &v);
		setBottom(v);
	}
	else if(hash == stringHash("width")) {
		float v = 0;
		sscanf(value, "%f", &v);
		setWidth((unsigned)v);
	}
	else if(hash == stringHash("height")) {
		float v = 0;
		sscanf(value, "%f", &v);
		setHeight((unsigned)v);
	}
	else if(hash == stringHash("transform")) {
		setTransform(value);
	}
	else if(hash == stringHash("-moz-transform")) {
		setTransform(value);
	}
	else if(hash == stringHash("transform-origin")) {
		setTransformOrigin(value);
	}
	else if(hash == stringHash("-moz-transform-origin")) {
		setTransformOrigin(value);
	}
	else if(hash == stringHash("background-image")) {
		setBackgroundImage(value);
	}
	else if(hash == stringHash("backgroundcolor")) {
		setBackgroundColor(value);
	}
	else if(hash == stringHash("background-position")) {
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
			const StringHash hash = stringLowerCaseHash(nameBegin, nameEnd - nameBegin);

			Mat44 mat;
			if(hash == stringHash("translate"))
			{
				if(valueIdx == 1) values[1] = 0;	// If the second param is not provided, assign zero
				mat = Mat44::makeTranslation(values);
			}
			else if(hash == stringHash("translatex"))
			{
				const float val[3] = { values[0], 0, 0 };
				mat = Mat44::makeTranslation(val);
			}
			else if(hash == stringHash("translatey"))
			{
				const float val[3] = { 0, values[0], 0 };
				mat = Mat44::makeTranslation(val);
			}
			else if(hash == stringHash("scale"))
			{
				if(valueIdx == 1) values[1] = values[0];	// If the second param is not provided, use the first one
				const float val[3] = { values[0], values[1], 1 };
				mat = Mat44::makeScale(val);
			}
			else if(hash == stringHash("scalex"))
			{
				const float val[3] = { values[0], 1, 1 };
				mat = Mat44::makeScale(val);
			}
			else if(hash == stringHash("scaley"))
			{
				const float val[3] = { 1, values[0], 1 };
				mat = Mat44::makeScale(val);
			}
			else if(hash == stringHash("rotate"))
			{
				static const float zaxis[3] = { 0, 0, 1 };
				mat = Mat44::makeAxisRotation(zaxis, values[0]);
			}
			else if(hash == stringHash("skew"))
			{
				RHASSERT(false && "Not implemented");
			}
			else if(hash == stringHash("skewx"))
			{
				RHASSERT(false && "Not implemented");
			}
			else if(hash == stringHash("skewy"))
			{
				RHASSERT(false && "Not implemented");
			}
			else if(hash == stringHash("matrix"))
			{
				RHASSERT(false && "Not implemented");
			}
			else
			{
				RHASSERT(false && "invalid transform function");
			}

			// Apply the transform with origin adjustment
			const Vec3 origin = style->origin();
			const float negOrigin[3] = { -origin.x, -origin.y, 0 };

			style->_localTransformation =
				style->_localTransformation *
				Mat44::makeTranslation(origin.data) *
				mat *
				Mat44::makeTranslation(negOrigin);
		}

		static void callback(ParserResult* result, Parser* parser)
		{
			ParserState* state = reinterpret_cast<ParserState*>(parser->userdata);
			ElementStyle* style = state->style;

			const StringHash hash = stringHash(result->type, 0);

			if(hash == stringHash("name")) {
				state->nameBegin = result->begin;
				state->nameEnd = result->end;
				state->valueIdx = 0;
				memset(state->values, 0, sizeof(state->values));
			}
			else if(hash == stringHash("value")) {
				char bk = *result->end;
				*const_cast<char*>(result->end) = '\0';
				sscanf(result->begin, "%f", &state->values[state->valueIdx]);
				*const_cast<char*>(result->end) = bk;
				++state->valueIdx;
			}
			else if(hash == stringHash("unit")) {
				// Convert deg to rad
				if(result->end - result->begin > 0 && state->valueIdx == 1 &&
					strncmp(result->begin, "deg", result->end - result->begin) == 0
				)
					state->values[state->valueIdx - 1] *= 3.1415926535897932f / 180;
			}
			else if(hash == stringHash("end")) {
				state->applyTransform();
			}
			else if(hash == stringHash("none")) {
				style->setIdentity();
			}
			else
				RHASSERT(false);
		}
	};	// ParserState

	ParserState state = { this, NULL, NULL, 0, {0} };
	Parser parser(transformStr, transformStr + strlen(transformStr), ParserState::callback, &state);
	Parsing::transform(&parser).any();
}

void ElementStyle::setTransformOrigin(const char* transformOriginStr)
{
	struct ParserState
	{
		ElementStyle* style;
		int valueIdx;
		float values[2];
		bool usePercentage[2];

		static void callback(ParserResult* result, Parser* parser)
		{
			ParserState* state = reinterpret_cast<ParserState*>(parser->userdata);
			ElementStyle* style = state->style;

			const StringHash hash = stringHash(result->type, 0);

			if(hash == stringHash("value")) {
				char bk = *result->end;
				*const_cast<char*>(result->end) = '\0';
				sscanf(result->begin, "%f", &state->values[state->valueIdx]);
				*const_cast<char*>(result->end) = bk;
				++state->valueIdx;
			}
			else if(hash == stringHash("unit")) {
				if(strncmp(result->begin, "px", result->end - result->begin) == 0)
					state->usePercentage[state->valueIdx-1] = false;
			}
			else if(hash == stringHash("named")) {
				const StringHash hash2 = stringHash(result->begin, result->end - result->begin);

				if(hash2 == stringHash("center"))
					state->values[state->valueIdx] = 50;
				if(hash2 == stringHash("left"))
					state->values[0] = 0;
				if(hash2 == stringHash("right"))
					state->values[0] = 100;
				if(hash2 == stringHash("top"))
					state->values[1] = 0;
				if(hash2 == stringHash("bottom"))
					state->values[1] = 100;

				++state->valueIdx;
			}
			else if(hash == stringHash("end")) {
				style->_origin.x = state->values[0];
				style->_origin.y = state->values[1];
				style->_originUsePercentage[0] = state->usePercentage[0];
				style->_originUsePercentage[1] = state->usePercentage[1];
			}
			else
				RHASSERT(false);
		}
	};	// ParserState

	ParserState state = { this, NULL, NULL, {0}, { true, true } };
	Parser parser(transformOriginStr, transformOriginStr + strlen(transformOriginStr), ParserState::callback, &state);
	Parsing::transformOrigin(&parser).once();
}

Vec3 ElementStyle::origin() const
{
	return Vec3(
		_originUsePercentage[0] ? _origin.x * 0.01f * width() : _origin.x,
		_originUsePercentage[1] ? _origin.y * 0.01f * height() : _origin.y,
		0
	);
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

			const StringHash hash = stringHash(result->type, 0);

			if(hash == stringHash("value")) {
				char bk = *result->end;
				*const_cast<char*>(result->end) = '\0';
				sscanf(result->begin, "%f", &state->values[state->valueIdx]);
				*const_cast<char*>(result->end) = bk;
				++state->valueIdx;
			}
			else if(hash == stringHash("unit")) {
			}
			else if(hash == stringHash("end")) {
				style->backgroundPositionX = state->values[0];
				style->backgroundPositionY = state->values[1];
			}
			else
				RHASSERT(false);
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
