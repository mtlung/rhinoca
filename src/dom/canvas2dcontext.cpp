#include "pch.h"
#include "canvas2dcontext.h"
#include "image.h"
#include "../mat44.h"
#include "../vec3.h"

using namespace Render;

namespace Dom {

JSClass CanvasRenderingContext2D::jsClass = {
	"CanvasRenderingContext2D", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub, JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSBool getCanvas(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	CanvasRenderingContext2D* self = reinterpret_cast<CanvasRenderingContext2D*>(JS_GetPrivate(cx, obj));
	*vp = OBJECT_TO_JSVAL(self->canvas->jsObject);
	return JS_TRUE;
}

static JSPropertySpec properties[] = {
	{"canvas", 0, 0, getCanvas, JS_PropertyStub},
	{0}
};

static JSBool clearRect(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	if(!JS_InstanceOf(cx, obj, &CanvasRenderingContext2D::jsClass, argv)) return JS_FALSE;
	CanvasRenderingContext2D* self = reinterpret_cast<CanvasRenderingContext2D*>(JS_GetPrivate(cx, obj));
	if(!self) return JS_FALSE;

	double x, y, w, h;
	JS_ValueToNumber(cx, argv[0], &x);
	JS_ValueToNumber(cx, argv[1], &y);
	JS_ValueToNumber(cx, argv[2], &w);
	JS_ValueToNumber(cx, argv[3], &h);

	self->clearRect((float)x, (float)y, (float)w, (float)h);

	return JS_TRUE;
}

static JSBool beginLayer(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	if(!JS_InstanceOf(cx, obj, &CanvasRenderingContext2D::jsClass, argv)) return JS_FALSE;
	CanvasRenderingContext2D* self = reinterpret_cast<CanvasRenderingContext2D*>(JS_GetPrivate(cx, obj));
	if(!self) return JS_FALSE;
	self->beginLayer();
	return JS_TRUE;
}

static JSBool endLayer(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	if(!JS_InstanceOf(cx, obj, &CanvasRenderingContext2D::jsClass, argv)) return JS_FALSE;
	CanvasRenderingContext2D* self = reinterpret_cast<CanvasRenderingContext2D*>(JS_GetPrivate(cx, obj));
	if(!self) return JS_FALSE;
	self->endLayer();
	return JS_TRUE;
}

static JSBool drawImage(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	if(!JS_InstanceOf(cx, obj, &CanvasRenderingContext2D::jsClass, argv)) return JS_FALSE;
	CanvasRenderingContext2D* self = reinterpret_cast<CanvasRenderingContext2D*>(JS_GetPrivate(cx, obj));
	if(!self) return JS_FALSE;

	JSObject* imgObj = NULL;
	JS_ValueToObject(cx, argv[0], &imgObj);

	// Determine the source is an image or a canvas
	Texture* texture;
	if(JS_InstanceOf(cx, imgObj, &HTMLImageElement::jsClass, NULL)) {
		HTMLImageElement* img = reinterpret_cast<HTMLImageElement*>(JS_GetPrivate(cx ,imgObj));
		texture = img->texture.get();
	}
	if(JS_InstanceOf(cx, imgObj, &HTMLCanvasElement::jsClass, NULL)) {
		HTMLCanvasElement* otherCanvas = reinterpret_cast<HTMLCanvasElement*>(JS_GetPrivate(cx ,imgObj));
		texture = otherCanvas->texture();
	}

	double sx, sy, sw, sh;	// Source x, y, width and height
	double dx, dy, dw, dh;	// Dest x, y, width and height

	switch(argc) {
	case 3:
		sx = sy = 0;
		sw = dw = texture->width;
		sh = dh = texture->height;
		JS_ValueToNumber(cx, argv[1], &dx);
		JS_ValueToNumber(cx, argv[2], &dy);
		break;
	case 5:
		sx = sy = 0;
		sw = texture->width;
		sh = texture->height;
		JS_ValueToNumber(cx, argv[1], &dx);
		JS_ValueToNumber(cx, argv[2], &dy);
		JS_ValueToNumber(cx, argv[3], &dw);
		JS_ValueToNumber(cx, argv[4], &dh);
		break;
	case 9:
		JS_ValueToNumber(cx, argv[1], &sx);
		JS_ValueToNumber(cx, argv[2], &sy);
		JS_ValueToNumber(cx, argv[3], &sw);
		JS_ValueToNumber(cx, argv[4], &sh);
		JS_ValueToNumber(cx, argv[5], &dx);
		JS_ValueToNumber(cx, argv[6], &dy);
		JS_ValueToNumber(cx, argv[7], &dw);
		JS_ValueToNumber(cx, argv[8], &dh);
		break;
	default:
		return JS_FALSE;
	}

	self->drawImage(texture,
		(float)sx, (float)sy, (float)sw, (float)sh,
		(float)dx, (float)dy, (float)dw, (float)dh
	);

	return JS_TRUE;
}

static JSBool save(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	if(!JS_InstanceOf(cx, obj, &CanvasRenderingContext2D::jsClass, argv)) return JS_FALSE;
	CanvasRenderingContext2D* self = reinterpret_cast<CanvasRenderingContext2D*>(JS_GetPrivate(cx, obj));
	if(!self) return JS_FALSE;

	self->save();

	return JS_TRUE;
}

static JSBool restore(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	if(!JS_InstanceOf(cx, obj, &CanvasRenderingContext2D::jsClass, argv)) return JS_FALSE;
	CanvasRenderingContext2D* self = reinterpret_cast<CanvasRenderingContext2D*>(JS_GetPrivate(cx, obj));
	if(!self) return JS_FALSE;

	self->restore();

	return JS_TRUE;
}

static JSBool scale(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	if(!JS_InstanceOf(cx, obj, &CanvasRenderingContext2D::jsClass, argv)) return JS_FALSE;
	CanvasRenderingContext2D* self = reinterpret_cast<CanvasRenderingContext2D*>(JS_GetPrivate(cx, obj));
	if(!self) return JS_FALSE;

	double x, y;
	JS_ValueToNumber(cx, argv[0], &x);
	JS_ValueToNumber(cx, argv[1], &y);
	self->scale((float)x, (float)y);

	return JS_TRUE;
}

static JSBool rotate(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	if(!JS_InstanceOf(cx, obj, &CanvasRenderingContext2D::jsClass, argv)) return JS_FALSE;
	CanvasRenderingContext2D* self = reinterpret_cast<CanvasRenderingContext2D*>(JS_GetPrivate(cx, obj));
	if(!self) return JS_FALSE;

	double angle;
	JS_ValueToNumber(cx, argv[0], &angle);
	self->rotate((float)angle);

	return JS_TRUE;
}

static JSBool translate(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	if(!JS_InstanceOf(cx, obj, &CanvasRenderingContext2D::jsClass, argv)) return JS_FALSE;
	CanvasRenderingContext2D* self = reinterpret_cast<CanvasRenderingContext2D*>(JS_GetPrivate(cx, obj));
	if(!self) return JS_FALSE;

	double x, y;
	JS_ValueToNumber(cx, argv[0], &x);
	JS_ValueToNumber(cx, argv[1], &y);
	self->translate((float)x, (float)y);

	return JS_TRUE;
}

static JSBool transform(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	if(!JS_InstanceOf(cx, obj, &CanvasRenderingContext2D::jsClass, argv)) return JS_FALSE;
	CanvasRenderingContext2D* self = reinterpret_cast<CanvasRenderingContext2D*>(JS_GetPrivate(cx, obj));
	if(!self) return JS_FALSE;

	double m11, m12, m21, m22, dx, dy;
	JS_ValueToNumber(cx, argv[0], &m11);
	JS_ValueToNumber(cx, argv[1], &m12);
	JS_ValueToNumber(cx, argv[2], &m21);
	JS_ValueToNumber(cx, argv[3], &m22);
	JS_ValueToNumber(cx, argv[4], &dx);
	JS_ValueToNumber(cx, argv[5], &dy);

	self->transform((float)m11, (float)m21, (float)m21, (float)m22, (float)dx, (float)dy);

	return JS_TRUE;
}

static JSBool setTransform(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	if(!JS_InstanceOf(cx, obj, &CanvasRenderingContext2D::jsClass, argv)) return JS_FALSE;
	CanvasRenderingContext2D* self = reinterpret_cast<CanvasRenderingContext2D*>(JS_GetPrivate(cx, obj));
	if(!self) return JS_FALSE;

	double m11, m12, m21, m22, dx, dy;
	JS_ValueToNumber(cx, argv[0], &m11);
	JS_ValueToNumber(cx, argv[1], &m12);
	JS_ValueToNumber(cx, argv[2], &m21);
	JS_ValueToNumber(cx, argv[3], &m22);
	JS_ValueToNumber(cx, argv[4], &dx);
	JS_ValueToNumber(cx, argv[5], &dy);

	self->setTransform((float)m11, (float)m21, (float)m21, (float)m22, (float)dx, (float)dy);

	return JS_TRUE;
}

static JSFunctionSpec methods[] = {
	{"clearRect", clearRect, 4,0,0},
	{"beginLayer", beginLayer, 0,0,0},
	{"endLayer", endLayer, 0,0,0},
	{"drawImage", drawImage, 5,0,0},
	{"save", save, 0,0,0},
	{"restore", restore, 0,0,0},
	{"scale", scale, 2,0,0},
	{"rotate", rotate, 1,0,0},
	{"translate", translate, 2,0,0},
	{"transform", transform, 6,0,0},
	{"setTransform", setTransform, 6,0,0},
	{0}
};

CanvasRenderingContext2D::CanvasRenderingContext2D(HTMLCanvasElement* c)
	: Context(c)
{
	currentState.transform = Mat44::identity;
}

CanvasRenderingContext2D::~CanvasRenderingContext2D()
{
}

void CanvasRenderingContext2D::bind(JSContext* cx, JSObject* parent)
{
	ASSERT(!jsContext);
	jsContext = cx;
	jsObject = JS_NewObject(cx, &jsClass, NULL, NULL);
	VERIFY(JS_SetPrivate(cx, jsObject, this));
	VERIFY(JS_DefineFunctions(cx, jsObject, methods));
	VERIFY(JS_DefineProperties(cx, jsObject, properties));
	addReference();
}

void CanvasRenderingContext2D::beginLayer()
{
}

void CanvasRenderingContext2D::endLayer()
{
}

void CanvasRenderingContext2D::clearRect(float x, float y, float w, float h)
{
	canvas->bindFramebuffer();

	unsigned w_ = width();
	unsigned h_ = height();

	static const Driver::BlendState blendState = {
		false,
		Driver::BlendState::Add, Driver::BlendState::Add,
		Driver::BlendState::One, Driver::BlendState::Zero,	// Color src, dst
		Driver::BlendState::One, Driver::BlendState::Zero	// Alpha src, dst
	};
	Driver::setBlendState(blendState);

	Driver::setViewport(0, 0, w_, h_);
	Driver::ortho(0, w_, 0, h_, 10, -10);
	Driver::setViewMatrix(Mat44::identity.data);

	Driver::setColor(0, 0, 0, 0);
	Driver::drawQuad(
		x + 0, y + 0,
		x + w, y + 0,
		x + w, y + h,
		x + 0, y + h,
		-1	// z value
	);
}

void CanvasRenderingContext2D::drawImage(
	Texture* texture,
	float dstx, float dsty)
{
	drawImage(
		texture,
		0, 0, (float)texture->width, (float)texture->height,
		dstx, dsty, (float)texture->width, (float)texture->height
	);
}

void CanvasRenderingContext2D::drawImage(
	Texture* texture,
	float dstx, float dsty, float dstw, float dsth)
{
	drawImage(texture,
		0, 0, (float)texture->width, (float)texture->height,
		dstx, dsty, dstw, dsth
	);
}

void CanvasRenderingContext2D::drawImage(
	Texture* texture,
	float srcx, float srcy, float srcw, float srch,
	float dstx, float dsty, float dstw, float dsth)
{
	canvas->bindFramebuffer();

	unsigned w = width();
	unsigned h = height();

	ASSERT(w > 0 && h > 0);

	static const Driver::BlendState blendState = {
		true,
		Driver::BlendState::Add, Driver::BlendState::Add,
		Driver::BlendState::SrcAlpha, Driver::BlendState::InvSrcAlpha,	// Color src, dst
		Driver::BlendState::SrcAlpha, Driver::BlendState::DstAlpha		// Alpha src, dst
	};
	Driver::setBlendState(blendState);

	Driver::setViewport(0, 0, w, h);
	Driver::ortho(0, w, 0, h, 10, -10);
	Driver::setViewMatrix(Mat44::identity.data);

	float tw = 1.0f / texture->width;
	float th = 1.0f / texture->height;

	srcx *= tw;	srcw *= tw;
	srcy *= th; srch *= th;

	texture->bind();

	float sx1 = srcx, sx2 = srcx + srcw;
	float sy1 = srcy, sy2 = srcy + srch;
	float dx1 = dstx, dx2 = dstx + dstw;
	float dy1 = dsty, dy2 = dsty + dsth;

	Vec3 dp1(dx1, dy1, 1);
	Vec3 dp2(dx2, dy1, 1);
	Vec3 dp3(dx2, dy2, 1);
	Vec3 dp4(dx1, dy2, 1);

	currentState.transform.transformPoint(dp1.data);
	currentState.transform.transformPoint(dp2.data);
	currentState.transform.transformPoint(dp3.data);
	currentState.transform.transformPoint(dp4.data);

	Driver::setColor(1, 1, 1, 1);
	Driver::drawQuad(
		dp1.x, dp1.y,
		dp2.x, dp2.y,
		dp3.x, dp3.y,
		dp4.x, dp4.y,
		1,	// z value
		sx1, sy1,
		sx2, sy1,
		sx2, sy2,
		sx1, sy2
	);
}

void CanvasRenderingContext2D::save()
{
	stateStack.push_back(currentState);
}

void CanvasRenderingContext2D::restore()
{
	currentState = stateStack.top();
	stateStack.pop_back();
}

void CanvasRenderingContext2D::scale(float x, float y)
{
	float v[3] = { x, y, 1 };
	Mat44 m = Mat44::makeScale(v);
	currentState.transform *= m;
}

void CanvasRenderingContext2D::rotate(float angle)
{
	float axis[3] = { 0, 0, 1 };
	Mat44 m = Mat44::makeAxisRotation(axis, angle);
	currentState.transform *= m;
}

void CanvasRenderingContext2D::translate(float x, float y)
{
	float v[3] = { x, y, 0 };
	Mat44 m = Mat44::makeTranslation(v);
	currentState.transform *= m;
}

void CanvasRenderingContext2D::transform(float m11, float m12, float m21, float m22, float dx, float dy)
{
	Mat44 m = Mat44::identity;
	m.m11 = m11;	m.m12 = m12;
	m.m21 = m21;	m.m22 = m22;
	m.m03 = dx;		m.m13 = dy;
	currentState.transform *= m;
}

void CanvasRenderingContext2D::setTransform(float m11, float m12, float m21, float m22, float dx, float dy)
{
	Mat44 m = Mat44::identity;
	m.m11 = m11;	m.m12 = m12;
	m.m21 = m21;	m.m22 = m22;
	m.m03 = dx;		m.m13 = dy;
	currentState.transform = m;
}

void CanvasRenderingContext2D::beginPath()
{
}

void CanvasRenderingContext2D::moveTo(float x, float y)
{
}

void CanvasRenderingContext2D::closePath()
{
}

void CanvasRenderingContext2D::lineTo(float x, float y)
{
}

void CanvasRenderingContext2D::quadrativeCureTo(float cpx, float cpy, float x, float y)
{
}

void CanvasRenderingContext2D::bezierCurveTo(float cp1x, float cp1y, float cp2x, float cp2y, float x, float y)
{
}

void CanvasRenderingContext2D::arcTo(float x1, float y1, float x2, float y2, float radius)
{
}

void CanvasRenderingContext2D::arc(float x, float y, float radius, float startAngle, float endAngle, bool anticlockwise)
{
}

void CanvasRenderingContext2D::rect(float x, float y, float w, float h)
{
}

void CanvasRenderingContext2D::fill()
{
}

void CanvasRenderingContext2D::stroke()
{
}

void CanvasRenderingContext2D::clip()
{
}

void CanvasRenderingContext2D::isPointInPath(float x, float y)
{
}

}	// namespace Dom
