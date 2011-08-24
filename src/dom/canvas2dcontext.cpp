#include "pch.h"
#include "canvas2dcontext.h"
#include "canvasgradient.h"
#include "color.h"
#include "image.h"
#include "imagedata.h"
#include "../mat44.h"
#include "../vec3.h"
#include "../render/vg/openvg.h"
#include "../render/vg/vgu.h"
#include <string.h>	// For strcasecmp

using namespace Render;

namespace Dom {

JSClass CanvasRenderingContext2D::jsClass = {
	"CanvasRenderingContext2D", JSCLASS_HAS_PRIVATE | JSCLASS_HAS_RESERVED_SLOTS(1),
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub, JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSBool getCanvas(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, obj);
	if(!self) return JS_FALSE;

	*vp = *self->canvas;
	return JS_TRUE;
}

static JSBool setGlobalAlpha(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, obj);
	if(!self) return JS_FALSE;

	double a;
	JS_ValueToNumber(cx, *vp, &a);
	self->setGlobalAlpha((float)a);
	return JS_TRUE;
}

static JSBool getStrokeStyle(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, obj);
	if(!self) return JS_FALSE;

	Color c;
	self->getStrokeColor((float*)(&c));

	char str[10];
	c.toString(str);

	*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, str));
	return JS_TRUE;
}

static JSBool setStrokeStyle(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, obj);
	if(!self) return JS_FALSE;

	if(JSVAL_IS_OBJECT(*vp)) {
		CanvasGradient* g = getJsBindable<CanvasGradient>(cx, JSVAL_TO_OBJECT(*vp));

		self->setStrokeGradient(g);
	}
	else {
		Color c;
		JsString jss(cx, *vp);
		if(c.parse(jss.c_str()))
			self->setStrokeColor((float*)&c);
		else {
			print(cx, "Fail to parse \"%s\" as a color\n", jss.c_str());
			return JS_FALSE;
		}
	}

	return JS_TRUE;
}

static JSBool getFillStyle(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, obj);
	if(!self) return JS_FALSE;

	Color c;
	self->getFillColor((float*)(&c));

	char str[10];
	c.toString(str);

	*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, str));
	return JS_TRUE;
}

static JSBool setFillStyle(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, obj);
	if(!self) return JS_FALSE;

	if(JSVAL_IS_OBJECT(*vp)) {
		CanvasGradient* g = getJsBindable<CanvasGradient>(cx, JSVAL_TO_OBJECT(*vp));
		if(!g) return JS_FALSE;

		self->setFillGradient(g);
	}
	else {
		Color c;
		JsString jss(cx, *vp);
		if(c.parse(jss.c_str()))
			self->setFillColor((float*)&c);
		else {
			print(cx, "Fail to parse \"%s\" as a color\n", jss.c_str());
			return JS_FALSE;
		}
	}

	return JS_TRUE;
}

static JSBool setLineCap(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, obj);
	if(!self) return JS_FALSE;

	JsString jss(cx, *vp);
	self->setLineCap(jss.c_str());

	return JS_TRUE;
}

static JSBool setLineJoin(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, obj);
	if(!self) return JS_FALSE;

	JsString jss(cx, *vp);
	self->setLineJoin(jss.c_str());

	return JS_TRUE;
}

static JSBool setLineWidth(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, obj);
	if(!self) return JS_FALSE;

	double w;
	JS_ValueToNumber(cx, *vp, &w);
	self->setLineWidth((float)w);
	return JS_TRUE;
}

static JSBool getFont(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, obj);
	if(!self) return JS_FALSE;

	*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, self->font().c_str()));
	return JS_TRUE;
}

static JSBool setFont(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, obj);
	if(!self) return JS_FALSE;

	JsString jss(cx, *vp);
	if(!jss) return JS_FALSE;

	self->setFont(jss.c_str());
	return JS_TRUE;
}

static JSBool getTextAlign(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, obj);
	if(!self) return JS_FALSE;

	*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, self->textAlign().c_str()));
	return JS_TRUE;
}

static JSBool setTextAlign(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, obj);
	if(!self) return JS_FALSE;

	JsString jss(cx, *vp);
	if(!jss) return JS_FALSE;

	self->setTextAlign(jss.c_str());
	return JS_TRUE;
}

static JSBool getTextBaseLine(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, obj);
	if(!self) return JS_FALSE;

	*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, self->textBaseLine().c_str()));
	return JS_TRUE;
}

static JSBool setTextBaseLine(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, obj);
	if(!self) return JS_FALSE;

	JsString jss(cx, *vp);
	if(!jss) return JS_FALSE;

	self->setTextBaseLine(jss.c_str());
	return JS_TRUE;
}

static JSBool setUseImgDimension(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, obj);
	if(!self) return JS_FALSE;

	JSBool b;
	if(!JS_ValueToBoolean(cx, *vp, &b))
		return JS_FALSE;

	self->useImgDimension = b;
	return JS_TRUE;
}

static JSPropertySpec properties[] = {
	{"canvas", 0, JSPROP_READONLY | JsBindable::jsPropFlags, getCanvas, JS_StrictPropertyStub},
	{"globalAlpha", 0, JsBindable::jsPropFlags, JS_PropertyStub, setGlobalAlpha},
	{"strokeStyle", 0, JsBindable::jsPropFlags, getStrokeStyle, setStrokeStyle},
	{"lineCap", 0, JsBindable::jsPropFlags, JS_PropertyStub, setLineCap},
	{"lineJoin", 0, JsBindable::jsPropFlags, JS_PropertyStub, setLineJoin},
	{"lineWidth", 0, JsBindable::jsPropFlags, JS_PropertyStub, setLineWidth},
	{"fillStyle", 0, JsBindable::jsPropFlags, getFillStyle, setFillStyle},
	{"font", 0, JsBindable::jsPropFlags, getFont, setFont},
	{"textAlign", 0, JsBindable::jsPropFlags, getTextAlign, setTextAlign},
	{"textBaseLine", 0, JsBindable::jsPropFlags, getTextBaseLine, setTextBaseLine},

	// Rhinoca extensions
	{"useImgDimension", 0, JsBindable::jsPropFlags, JS_PropertyStub, setUseImgDimension},	// Consider <IMG>'s width and height when in drawImage()

	{0}
};

static JSBool clearRect(JSContext* cx, uintN argc, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, vp);
	if(!self) return JS_FALSE;

	double x, y, w, h;
	VERIFY(JS_ValueToNumber(cx, JS_ARGV0, &x));
	VERIFY(JS_ValueToNumber(cx, JS_ARGV1, &y));
	VERIFY(JS_ValueToNumber(cx, JS_ARGV2, &w));
	VERIFY(JS_ValueToNumber(cx, JS_ARGV3, &h));

	self->clearRect((float)x, (float)y, (float)w, (float)h);

	return JS_TRUE;
}

static JSBool beginLayer(JSContext* cx, uintN argc, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, vp);
	if(!self) return JS_FALSE;

	self->beginLayer();
	return JS_TRUE;
}

static JSBool endLayer(JSContext* cx, uintN argc, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, vp);
	if(!self) return JS_FALSE;

	self->endLayer();
	return JS_TRUE;
}

static JSBool drawImage(JSContext* cx, uintN argc, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, vp);
	if(!self) return JS_FALSE;

	// Determine the source is an image or a canvas
	Texture* texture = NULL;

	int filter;
	unsigned imgw, imgh;

	if(HTMLImageElement* img = getJsBindableExactTypeNoThrow<HTMLImageElement>(cx, vp, 0)) {
		texture = img->texture.get();
		filter = img->filter;
		imgw = img->width();
		imgh = img->height();
	}
	else if(HTMLCanvasElement* otherCanvas = getJsBindable<HTMLCanvasElement>(cx, vp, 0)) {
		texture = otherCanvas->texture();
		filter = Driver::SamplerState::MIN_MAG_LINEAR;
		imgw = otherCanvas->width();
		imgh = otherCanvas->height();
	}

	if(!texture) return JS_FALSE;

	if(!self->useImgDimension) {
		imgw = texture->virtualWidth;
		imgh = texture->virtualHeight;
	}

	const float scalex = float(texture->virtualWidth) / imgw;
	const float scaley = float(texture->virtualHeight) / imgh;
	double sx, sy, sw, sh;	// Source x, y, width and height
	double dx, dy, dw, dh;	// Dest x, y, width and height

	switch(argc) {
	case 3:
		sx = sy = 0;
		sw = texture->virtualWidth;
		sh = texture->virtualHeight;
		dw = imgw;
		dh = imgh;
		VERIFY(JS_ValueToNumber(cx, JS_ARGV1, &dx));
		VERIFY(JS_ValueToNumber(cx, JS_ARGV2, &dy));
		break;
	case 5:
		sx = sy = 0;
		sw = texture->virtualWidth;
		sh = texture->virtualHeight;
		VERIFY(JS_ValueToNumber(cx, JS_ARGV1, &dx));
		VERIFY(JS_ValueToNumber(cx, JS_ARGV2, &dy));
		VERIFY(JS_ValueToNumber(cx, JS_ARGV3, &dw));
		VERIFY(JS_ValueToNumber(cx, JS_ARGV4, &dh));
		break;
	case 9:
		VERIFY(JS_ValueToNumber(cx, JS_ARGV1, &sx));
		VERIFY(JS_ValueToNumber(cx, JS_ARGV2, &sy));
		VERIFY(JS_ValueToNumber(cx, JS_ARGV3, &sw));
		VERIFY(JS_ValueToNumber(cx, JS_ARGV4, &sh));
		VERIFY(JS_ValueToNumber(cx, JS_ARGV5, &dx));
		VERIFY(JS_ValueToNumber(cx, JS_ARGV6, &dy));
		VERIFY(JS_ValueToNumber(cx, JS_ARGV7, &dw));
		VERIFY(JS_ValueToNumber(cx, JS_ARGV8, &dh));

		sx *= scalex;
		sy *= scaley;
		sw *= scalex;
		sh *= scaley;
		break;
	default:
		return JS_FALSE;
	}

	self->drawImage(
		texture, filter,
		(float)sx, (float)sy, (float)sw, (float)sh,
		(float)dx, (float)dy, (float)dw, (float)dh
	);

	return JS_TRUE;
}

static JSBool save(JSContext* cx, uintN argc, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, vp);
	if(!self) return JS_FALSE;

	self->save();

	return JS_TRUE;
}

static JSBool restore(JSContext* cx, uintN argc, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, vp);
	if(!self) return JS_FALSE;

	self->restore();

	return JS_TRUE;
}

static JSBool scale(JSContext* cx, uintN argc, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, vp);
	if(!self) return JS_FALSE;

	double x, y;
	VERIFY(JS_ValueToNumber(cx, JS_ARGV0, &x));
	VERIFY(JS_ValueToNumber(cx, JS_ARGV1, &y));
	self->scale((float)x, (float)y);

	return JS_TRUE;
}

static JSBool rotate(JSContext* cx, uintN argc, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, vp);
	if(!self) return JS_FALSE;

	double angle;
	VERIFY(JS_ValueToNumber(cx, JS_ARGV0, &angle));
	self->rotate((float)angle);

	return JS_TRUE;
}

static JSBool translate(JSContext* cx, uintN argc, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, vp);
	if(!self) return JS_FALSE;

	double x, y;
	VERIFY(JS_ValueToNumber(cx, JS_ARGV0, &x));
	VERIFY(JS_ValueToNumber(cx, JS_ARGV1, &y));
	self->translate((float)x, (float)y);

	return JS_TRUE;
}

static JSBool transform(JSContext* cx, uintN argc, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, vp);
	if(!self) return JS_FALSE;

	double m11, m12, m21, m22, dx, dy;
	VERIFY(JS_ValueToNumber(cx, JS_ARGV0, &m11));
	VERIFY(JS_ValueToNumber(cx, JS_ARGV1, &m12));
	VERIFY(JS_ValueToNumber(cx, JS_ARGV2, &m21));
	VERIFY(JS_ValueToNumber(cx, JS_ARGV3, &m22));
	VERIFY(JS_ValueToNumber(cx, JS_ARGV4, &dx));
	VERIFY(JS_ValueToNumber(cx, JS_ARGV5, &dy));

	self->transform((float)m11, (float)m21, (float)m21, (float)m22, (float)dx, (float)dy);

	return JS_TRUE;
}

static JSBool setTransform(JSContext* cx, uintN argc, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, vp);
	if(!self) return JS_FALSE;

	double m11, m12, m21, m22, dx, dy;
	VERIFY(JS_ValueToNumber(cx, JS_ARGV0, &m11));
	VERIFY(JS_ValueToNumber(cx, JS_ARGV1, &m12));
	VERIFY(JS_ValueToNumber(cx, JS_ARGV2, &m21));
	VERIFY(JS_ValueToNumber(cx, JS_ARGV3, &m22));
	VERIFY(JS_ValueToNumber(cx, JS_ARGV4, &dx));
	VERIFY(JS_ValueToNumber(cx, JS_ARGV5, &dy));

	self->setTransform((float)m11, (float)m21, (float)m21, (float)m22, (float)dx, (float)dy);

	return JS_TRUE;
}

static JSBool beginPath(JSContext* cx, uintN argc, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, vp);
	if(!self) return JS_FALSE;

	self->beginPath();

	return JS_TRUE;
}

static JSBool closePath(JSContext* cx, uintN argc, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, vp);
	if(!self) return JS_FALSE;

	self->closePath();

	return JS_TRUE;
}

static JSBool moveTo(JSContext* cx, uintN argc, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, vp);
	if(!self) return JS_FALSE;

	double x, y;
	VERIFY(JS_ValueToNumber(cx, JS_ARGV0, &x));
	VERIFY(JS_ValueToNumber(cx, JS_ARGV1, &y));
	self->moveTo((float)x, (float)y);

	return JS_TRUE;
}

static JSBool lineTo(JSContext* cx, uintN argc, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, vp);
	if(!self) return JS_FALSE;

	double x, y;
	VERIFY(JS_ValueToNumber(cx, JS_ARGV0, &x));
	VERIFY(JS_ValueToNumber(cx, JS_ARGV1, &y));
	self->lineTo((float)x, (float)y);

	return JS_TRUE;
}

static JSBool quadraticCurveTo(JSContext* cx, uintN argc, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, vp);
	if(!self) return JS_FALSE;

	double cpx, cpy, x, y;
	VERIFY(JS_ValueToNumber(cx, JS_ARGV0, &cpx));
	VERIFY(JS_ValueToNumber(cx, JS_ARGV1, &cpy));
	VERIFY(JS_ValueToNumber(cx, JS_ARGV2, &x));
	VERIFY(JS_ValueToNumber(cx, JS_ARGV3, &y));
	self->quadraticCurveTo((float)cpx, (float)cpy, (float)x, (float)y);

	return JS_TRUE;
}

static JSBool bezierCurveTo(JSContext* cx, uintN argc, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, vp);
	if(!self) return JS_FALSE;

	double cp1x, cp1y, cp2x, cp2y, x, y;
	VERIFY(JS_ValueToNumber(cx, JS_ARGV0, &cp1x));
	VERIFY(JS_ValueToNumber(cx, JS_ARGV1, &cp1y));
	VERIFY(JS_ValueToNumber(cx, JS_ARGV2, &cp2x));
	VERIFY(JS_ValueToNumber(cx, JS_ARGV3, &cp2y));
	VERIFY(JS_ValueToNumber(cx, JS_ARGV4, &x));
	VERIFY(JS_ValueToNumber(cx, JS_ARGV5, &y));
	self->bezierCurveTo((float)cp1x, (float)cp1y, (float)cp2x, (float)cp2y, (float)x, (float)y);

	return JS_TRUE;
}

static JSBool arc(JSContext* cx, uintN argc, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, vp);
	if(!self) return JS_FALSE;

	bool antiClockwise;
	double x, y, radius, startAngle, endAngle;
	VERIFY(JS_ValueToNumber(cx, JS_ARGV0, &x));
	VERIFY(JS_ValueToNumber(cx, JS_ARGV1, &y));
	VERIFY(JS_ValueToNumber(cx, JS_ARGV2, &radius));
	VERIFY(JS_ValueToNumber(cx, JS_ARGV3, &startAngle));
	VERIFY(JS_ValueToNumber(cx, JS_ARGV4, &endAngle));
	VERIFY(JS_GetValue(cx, JS_ARGV5, antiClockwise));
	self->arc((float)x, (float)y, (float)radius, (float)startAngle, (float)endAngle, antiClockwise);

	return JS_TRUE;
}

static JSBool rect(JSContext* cx, uintN argc, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, vp);
	if(!self) return JS_FALSE;

	double x, y, w, h;
	VERIFY(JS_ValueToNumber(cx, JS_ARGV0, &x));
	VERIFY(JS_ValueToNumber(cx, JS_ARGV1, &y));
	VERIFY(JS_ValueToNumber(cx, JS_ARGV2, &w));
	VERIFY(JS_ValueToNumber(cx, JS_ARGV3, &h));
	self->rect((float)x, (float)y, (float)w, (float)h);

	return JS_TRUE;
}

static JSBool createLinearGradient(JSContext* cx, uintN argc, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, vp);
	if(!self) return JS_FALSE;

	CanvasGradient* g = new CanvasGradient;
	g->bind(cx, NULL);

	double x1, y1, x2, y2;
	VERIFY(JS_ValueToNumber(cx, JS_ARGV0, &x1));
	VERIFY(JS_ValueToNumber(cx, JS_ARGV1, &y1));
	VERIFY(JS_ValueToNumber(cx, JS_ARGV2, &x2));
	VERIFY(JS_ValueToNumber(cx, JS_ARGV3, &y2));

	g->createLinear((float)x1, (float)y1, (float)x2, (float)y2);

	JS_RVAL(cx, vp) = *g;

	return JS_TRUE;
}

static JSBool createRadialGradient(JSContext* cx, uintN argc, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, vp);
	if(!self) return JS_FALSE;

	CanvasGradient* g = new CanvasGradient;
	g->bind(cx, NULL);

	double x1, y1, r1, x2, y2, r2;
	VERIFY(JS_ValueToNumber(cx, JS_ARGV0, &x1));
	VERIFY(JS_ValueToNumber(cx, JS_ARGV1, &y1));
	VERIFY(JS_ValueToNumber(cx, JS_ARGV2, &r1));
	VERIFY(JS_ValueToNumber(cx, JS_ARGV3, &x2));
	VERIFY(JS_ValueToNumber(cx, JS_ARGV4, &y2));
	VERIFY(JS_ValueToNumber(cx, JS_ARGV5, &r2));

	g->createRadial((float)x1, (float)y1, (float)r1, (float)x2, (float)y2, (float)r2);

	JS_RVAL(cx, vp) = *g;

	return JS_TRUE;
}

static JSBool stroke(JSContext* cx, uintN argc, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, vp);
	if(!self) return JS_FALSE;

	self->stroke();

	return JS_TRUE;
}

static JSBool strokeRect(JSContext* cx, uintN argc, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, vp);
	if(!self) return JS_FALSE;

	double x, y, w, h;
	VERIFY(JS_ValueToNumber(cx, JS_ARGV0, &x));
	VERIFY(JS_ValueToNumber(cx, JS_ARGV1, &y));
	VERIFY(JS_ValueToNumber(cx, JS_ARGV2, &w));
	VERIFY(JS_ValueToNumber(cx, JS_ARGV3, &h));
	self->strokeRect((float)x, (float)y, (float)w, (float)h);

	return JS_TRUE;
}

static JSBool fill(JSContext* cx, uintN argc, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, vp);
	if(!self) return JS_FALSE;

	self->fill();

	return JS_TRUE;
}

static JSBool fillRect(JSContext* cx, uintN argc, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, vp);
	if(!self) return JS_FALSE;

	double x, y, w, h;
	VERIFY(JS_ValueToNumber(cx, JS_ARGV0, &x));
	VERIFY(JS_ValueToNumber(cx, JS_ARGV1, &y));
	VERIFY(JS_ValueToNumber(cx, JS_ARGV2, &w));
	VERIFY(JS_ValueToNumber(cx, JS_ARGV3, &h));
	self->fillRect((float)x, (float)y, (float)w, (float)h);

	return JS_TRUE;
}

static JSBool createImageData(JSContext* cx, uintN argc, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, vp);
	if(!self) return JS_FALSE;

	ImageData* imgData = getJsBindable<ImageData>(cx, vp, 0);

	if(imgData)
		imgData = self->createImageData(cx, imgData);
	else if(argc >= 2) {
		int32 sw, sh;
		if(JS_ValueToInt32(cx, JS_ARGV0, &sw) && JS_ValueToInt32(cx, JS_ARGV1, &sh))
			imgData = self->createImageData(cx, sw, sh);
	}

	return imgData ? JS_TRUE : JS_FALSE;
}

static JSBool getImageData(JSContext* cx, uintN argc, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, vp);
	if(!self) return JS_FALSE;

	int32 x, y, w, h;
	VERIFY(JS_ValueToInt32(cx, JS_ARGV0, &x));
	VERIFY(JS_ValueToInt32(cx, JS_ARGV1, &y));
	VERIFY(JS_ValueToInt32(cx, JS_ARGV2, &w));
	VERIFY(JS_ValueToInt32(cx, JS_ARGV3, &h));
	ImageData* imgData = self->getImageData(cx, x, y, w, h);

	JS_RVAL(cx, vp) = *imgData;

	return JS_TRUE;
}

static JSBool putImageData(JSContext* cx, uintN argc, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, vp);
	if(!self) return JS_FALSE;

	ImageData* imgData = getJsBindable<ImageData>(cx, vp, 0);
	if(!imgData) return JS_FALSE;

	int32 dx, dy;
	int32 dirtyX = 0, dirtyY = 0;
	int32 dirtyWidth = imgData->width;
	int32 dirtyHeight = imgData->height;

	VERIFY(JS_ValueToInt32(cx, JS_ARGV1, &dx));
	VERIFY(JS_ValueToInt32(cx, JS_ARGV2, &dy));

	if(argc >= 7) {
		// TODO: Deal with negative values, as state in the spec
		VERIFY(JS_ValueToInt32(cx, JS_ARGV3, &dirtyX));
		VERIFY(JS_ValueToInt32(cx, JS_ARGV4, &dirtyY));
		VERIFY(JS_ValueToInt32(cx, JS_ARGV5, &dirtyWidth));
		VERIFY(JS_ValueToInt32(cx, JS_ARGV6, &dirtyHeight));
	}

	self->putImageData(imgData, dx, dy, dirtyX, dirtyY, dirtyWidth, dirtyHeight);

	return JS_TRUE;
}

static JSBool fillText(JSContext* cx, uintN argc, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, vp);
	if(!self) return JS_FALSE;

	return JS_TRUE;
}

static JSBool strokeText(JSContext* cx, uintN argc, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, vp);
	if(!self) return JS_FALSE;

	return JS_TRUE;
}

static JSBool measureText(JSContext* cx, uintN argc, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, vp);
	if(!self) return JS_FALSE;

	return JS_TRUE;
}

static JSBool beginBatch(JSContext* cx, uintN argc, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, vp);
	if(!self) return JS_FALSE;

	self->beginBatch();

	return JS_TRUE;
}

static JSBool endBatch(JSContext* cx, uintN argc, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, vp);
	if(!self) return JS_FALSE;

	self->endBatch();

	return JS_TRUE;
}

static JSFunctionSpec methods[] = {
	{"clearRect", clearRect, 4,0},
	{"beginLayer", beginLayer, 0,0},
	{"endLayer", endLayer, 0,0},
	{"drawImage", drawImage, 5,0},

	{"save", save, 0,0},
	{"restore", restore, 0,0},

	{"scale", scale, 2,0},
	{"rotate", rotate, 1,0},
	{"translate", translate, 2,0},
	{"transform", transform, 6,0},
	{"setTransform", setTransform, 6,0},

	{"beginPath", beginPath, 0,0},
	{"closePath", closePath, 0,0},
	{"moveTo", moveTo, 2,0},
	{"lineTo", lineTo, 2,0},
	{"quadraticCurveTo", quadraticCurveTo, 4,0},
	{"bezierCurveTo", bezierCurveTo, 6,0},
	{"arc", arc, 6,0},
	{"rect", rect, 4,0},

	{"createLinearGradient", createLinearGradient, 4,0},
	{"createRadialGradient", createRadialGradient, 6,0},

	{"stroke", stroke, 0,0},
	{"strokeRect", strokeRect, 4,0},
	{"fill", fill, 0,0},
	{"fillRect", fillRect, 4,0},

	{"createImageData", createImageData, 1,0},
	{"getImageData", getImageData, 4,0},
	{"putImageData", putImageData, 3,0},

	{"fillText", fillText, 4,0},
	{"strokeText", strokeText, 4,0},
	{"measureText", measureText, 1,0},

	// Our own batch optimization extension to canvas
	{"beginBatch", beginBatch, 0,0},
	{"endBatch", endBatch, 0,0},

	{0}
};


struct CanvasRenderingContext2D::OpenVG
{
	VGPath path;			/// The path for generic use
	bool pathEmpty;
	VGPath pathSimpleShape;	/// The path allocated for simple shape usage
	VGPaint strokePaint;
	VGPaint fillPaint;
};

CanvasRenderingContext2D::CanvasRenderingContext2D(HTMLCanvasElement* c)
	: Context(c)
	, _globalAlpha(1)
	, useImgDimension(false)
{
	currentState.transform = Mat44::identity;

	openvg = new OpenVG;

	openvg->path = vgCreatePath(
		VG_PATH_FORMAT_STANDARD, VG_PATH_DATATYPE_F,
		1, 0, 0, 0, VG_PATH_CAPABILITY_ALL
	);

	openvg->pathSimpleShape = vgCreatePath(
		VG_PATH_FORMAT_STANDARD, VG_PATH_DATATYPE_F,
		1, 0, 0, 0, VG_PATH_CAPABILITY_ALL
	);

	openvg->strokePaint = vgCreatePaint();
	openvg->fillPaint = vgCreatePaint();
	vgSetPaint(openvg->strokePaint, VG_STROKE_PATH);
	vgSetPaint(openvg->fillPaint, VG_FILL_PATH);

	vgSeti(VG_FILL_RULE, VG_NON_ZERO);
	vgSetf(VG_STROKE_LINE_WIDTH, 1);
}

CanvasRenderingContext2D::~CanvasRenderingContext2D()
{
	if(openvg->path)
		vgDestroyPath(openvg->path);
	vgDestroyPaint(openvg->strokePaint);
	vgDestroyPaint(openvg->fillPaint);

	delete openvg;
}

void CanvasRenderingContext2D::bind(JSContext* cx, JSObject* parent)
{
	ASSERT(!jsContext);
	jsContext = cx;
	jsObject = JS_NewObject(cx, &jsClass, NULL, parent);
	VERIFY(JS_SetPrivate(cx, *this, this));
	VERIFY(JS_DefineFunctions(cx, *this, methods));
	VERIFY(JS_DefineProperties(cx, *this, properties));
	addReference();	// releaseReference() in JsBindable::finalize()
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
		Driver::BlendState::One, Driver::BlendState::One,	// Alpha src, dst
		Driver::BlendState::EnableAll
	};
	Driver::setBlendState(blendState);

	Driver::ViewportState vs1, vs2 = { 0, 0, (unsigned)w, (unsigned)h };
	Driver::getViewportState(vs1);
	if(memcmp(&vs1, &vs2, sizeof(vs1)) != 0) { 
		Driver::setViewport(0, 0, w_, h_);
		Driver::ortho(0, w_, 0, h_, 10, -10);
	}

	Driver::setViewMatrix(Mat44::identity.data);

	Driver::drawQuad(
		x + 0, y + 0,
		x + w, y + 0,
		x + w, y + h,
		x + 0, y + h,
		-1,	// z value
		(rhuint8)0, (rhuint8)0, (rhuint8)0, (rhuint8)0	// Clear as transparent black
	);
}

static JSBool construct(JSContext* cx, uintN argc, jsval* vp)
{
	ASSERT(false && "For compatible with javascript instanceof operator only, you are not suppose to new a CanvasRenderingContext2D directly");
	return JS_FALSE;
}

void CanvasRenderingContext2D::registerClass(JSContext* cx, JSObject* parent)
{
	VERIFY(JS_InitClass(cx, parent, NULL, &jsClass, &construct, 0, NULL, NULL, NULL, NULL));
}

void CanvasRenderingContext2D::drawImage(
	Texture* texture, int filter,
	float dstx, float dsty)
{
	drawImage(
		texture, filter,
		0, 0, (float)texture->virtualWidth, (float)texture->virtualHeight,
		dstx, dsty, (float)texture->virtualWidth, (float)texture->virtualHeight
	);
}

void CanvasRenderingContext2D::drawImage(
	Texture* texture, int filter,
	float dstx, float dsty, float dstw, float dsth)
{
	drawImage(
		texture, filter,
		0, 0, (float)texture->virtualWidth, (float)texture->virtualHeight,
		dstx, dsty, dstw, dsth
	);
}

void CanvasRenderingContext2D::drawImage(
	Texture* texture, int filter,
	float srcx, float srcy, float srcw, float srch,
	float dstx, float dsty, float dstw, float dsth)
{
/*	printf("draw to %x: %s, %.0f,%.0f,%.0f,%.0f,%.0f,%.0f,%.0f,%.0f\n",
		(unsigned)canvas->_framebuffer.handle, texture->uri().c_str(),
		srcx, srcy, srcw, srch,
		dstx, dsty, dstw, dsth
	);*/

	canvas->bindFramebuffer();

	unsigned w = width();
	unsigned h = height();

	ASSERT(w > 0 && h > 0);

	static const Driver::BlendState blendState = {
		true,
		Driver::BlendState::Add, Driver::BlendState::Add,
		Driver::BlendState::SrcAlpha, Driver::BlendState::InvSrcAlpha,	// Color src, dst
		Driver::BlendState::One, Driver::BlendState::One,		// Alpha src, dst
		Driver::BlendState::EnableAll
	};
	Driver::setBlendState(blendState);

	Driver::setViewport(0, 0, w, h);
	Driver::ortho(0, w, 0, h, 10, -10);
	Driver::setViewMatrix(Mat44::identity.data);

	// Delta UV per pixel
	float tw = 1.0f / texture->width;
	float th = 1.0f / texture->height;

	srcx *= tw; srcw *= tw;
	srcy *= th; srch *= th;

	// Use the texture
	ASSERT(filter >= 0 && filter <= Driver::SamplerState::MIP_MAG_LINEAR);
	Driver::SamplerState state = {
		texture->handle, (Render::Driver::SamplerState::Filter)filter,
		Driver::SamplerState::Edge,
		Driver::SamplerState::Edge,
	};
	Driver::setSamplerState(0, state);
	texture->hotness++;

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

	Driver::drawQuad(
		dp1.x, dp1.y,
		dp2.x, dp2.y,
		dp3.x, dp3.y,
		dp4.x, dp4.y,
		1,	// z value
		sx1, sy1,
		sx2, sy1,
		sx2, sy2,
		sx1, sy2,
		255, 255, 255, (rhuint8)(_globalAlpha * 255)
	);
}

void CanvasRenderingContext2D::save()
{
	stateStack.push_back(currentState);
}

void CanvasRenderingContext2D::restore()
{
	if(stateStack.empty()) return;	// Be more forgiving
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

void CanvasRenderingContext2D::setTransform(float mat44[16])
{
	currentState.transform.copyFrom(mat44);
}

void CanvasRenderingContext2D::beginPath()
{
	vgClearPath(openvg->path, VG_PATH_CAPABILITY_ALL);
	openvg->pathEmpty = true;
}

void CanvasRenderingContext2D::closePath()
{
	VGubyte seg = VG_CLOSE_PATH;
	VGfloat data[] = { 0, 0 };
	vgAppendPathData(openvg->path, 1, &seg, data);
}

void CanvasRenderingContext2D::moveTo(float x, float y)
{
	VGubyte seg = VG_MOVE_TO;
	VGfloat data[] = { x, y };
	vgAppendPathData(openvg->path, 1, &seg, data);

	openvg->pathEmpty = false;
}

void CanvasRenderingContext2D::lineTo(float x, float y)
{
	VGubyte seg = openvg->pathEmpty ? VG_MOVE_TO : VG_LINE_TO_ABS;
	VGfloat data[] = { x, y };
	vgAppendPathData(openvg->path, 1, &seg, data);

	openvg->pathEmpty = false;
}

void CanvasRenderingContext2D::quadraticCurveTo(float cpx, float cpy, float x, float y)
{
	VGubyte seg = VG_QUAD_TO;
	VGfloat data[] = { cpx, cpy, x, y };
	vgAppendPathData(openvg->path, 1, &seg, data);

	openvg->pathEmpty = false;
}

void CanvasRenderingContext2D::bezierCurveTo(float cp1x, float cp1y, float cp2x, float cp2y, float x, float y)
{
	VGubyte seg = VG_CUBIC_TO;
	VGfloat data[] = { cp1x, cp1y, cp2x, cp2y, x, y };
	vgAppendPathData(openvg->path, 1, &seg, data);

	openvg->pathEmpty = false;
}

void CanvasRenderingContext2D::arcTo(float x1, float y1, float x2, float y2, float radius)
{
	// TODO: To be implement
}

void CanvasRenderingContext2D::arc(float x, float y, float radius, float startAngle, float endAngle, bool anticlockwise)
{
	startAngle = startAngle * 360 / (2 * 3.14159f);
	endAngle = endAngle * 360 / (2 * 3.14159f);
	radius *= 2;
	vguArc(openvg->path, x,y, radius,radius, startAngle, (anticlockwise ? -1 : 1) * (endAngle-startAngle), VGU_ARC_OPEN);
}

void CanvasRenderingContext2D::rect(float x, float y, float w, float h)
{
	vguRect(openvg->path, x, y, w, h);
	moveTo(x, y);
}

static const Driver::SamplerState noTexture = {
	NULL,
	Driver::SamplerState::MIP_MAG_LINEAR,
	Driver::SamplerState::Edge,
	Driver::SamplerState::Edge,
};

void CanvasRenderingContext2D::stroke()
{
	canvas->bindFramebuffer();
	Driver::setSamplerState(0, noTexture);
	vgResizeSurfaceSH(this->width(), this->height());

	const Mat44& m = currentState.transform;
	float mat33[] = {
		m.m00, m.m10, m.m20,
		m.m01, m.m11, m.m21,
		m.m03, m.m13, m.m33,
	};

	vgLoadMatrix(mat33);
	vgDrawPath(openvg->path, VG_STROKE_PATH);
}

void CanvasRenderingContext2D::strokeRect(float x, float y, float w, float h)
{
	vgClearPath(openvg->pathSimpleShape, VG_PATH_CAPABILITY_ALL);
	vguRect(openvg->pathSimpleShape, x, y, w, h);

	canvas->bindFramebuffer();
	Driver::setSamplerState(0, noTexture);
	vgResizeSurfaceSH(this->width(), this->height());

	const Mat44& m = currentState.transform;
	float mat33[] = {
		m.m00, m.m10, m.m20,
		m.m01, m.m11, m.m21,
		m.m03, m.m13, m.m33,
	};

	vgLoadMatrix(mat33);
	vgDrawPath(openvg->pathSimpleShape, VG_STROKE_PATH);
}

void CanvasRenderingContext2D::fill()
{
	canvas->bindFramebuffer();
	Driver::setSamplerState(0, noTexture);
	vgResizeSurfaceSH(this->width(), this->height());

	const Mat44& m = currentState.transform;
	float mat33[] = {
		m.m00, m.m10, m.m20,
		m.m01, m.m11, m.m21,
		m.m03, m.m13, m.m33,
	};

	vgLoadMatrix(mat33);
	vgDrawPath(openvg->path, VG_FILL_PATH);
}

void CanvasRenderingContext2D::fillRect(float x, float y, float w, float h)
{
	vgClearPath(openvg->pathSimpleShape, VG_PATH_CAPABILITY_ALL);
	vguRect(openvg->pathSimpleShape, x, y, w, h);

	canvas->bindFramebuffer();
	Driver::setSamplerState(0, noTexture);
	vgResizeSurfaceSH(this->width(), this->height());

	const Mat44& m = currentState.transform;
	float mat33[] = {
		m.m00, m.m10, m.m20,
		m.m01, m.m11, m.m21,
		m.m03, m.m13, m.m33,
	};

	vgLoadMatrix(mat33);
	vgDrawPath(openvg->pathSimpleShape, VG_FILL_PATH);
}

void CanvasRenderingContext2D::clip()
{
	// TODO: To be implement
}

void CanvasRenderingContext2D::isPointInPath(float x, float y)
{
	// TODO: To be implement
}

void CanvasRenderingContext2D::getStrokeColor(float* rgba)
{
	vgGetParameterfv(openvg->strokePaint, VG_PAINT_COLOR, 4, rgba);
}

void CanvasRenderingContext2D::setStrokeColor(float* rgba)
{
	vgSetPaint(openvg->strokePaint, VG_STROKE_PATH);
	vgSetParameterfv(openvg->strokePaint, VG_PAINT_COLOR, 4, rgba);
}

void CanvasRenderingContext2D::setStrokeGradient(CanvasGradient* gradient)
{
	vgSetParameterfv(
		gradient->handle, VG_PAINT_COLOR_RAMP_STOPS,
		gradient->stopCount * 5, gradient->stops
	);
	vgSetPaint(gradient->handle, VG_STROKE_PATH);
}

void CanvasRenderingContext2D::getFillColor(float* rgba)
{
	vgGetParameterfv(openvg->fillPaint, VG_PAINT_COLOR, 4, rgba);
}

void CanvasRenderingContext2D::setFillColor(const float* rgba)
{
	vgSetPaint(openvg->fillPaint, VG_FILL_PATH);
	vgSetParameterfv(openvg->fillPaint, VG_PAINT_COLOR, 4, rgba);
}

void CanvasRenderingContext2D::setFillGradient(CanvasGradient* gradient)
{
	vgSetParameterfv(
		gradient->handle, VG_PAINT_COLOR_RAMP_STOPS,
		gradient->stopCount * 5, gradient->stops
	);
	vgSetPaint(gradient->handle, VG_FILL_PATH);
}

void CanvasRenderingContext2D::setLineCap(const char* cap)
{
	struct Cap {
		const char* name;
		VGCapStyle style;
	};

	static const Cap caps[] = {
		{ "butt",	VG_CAP_BUTT },
		{ "round",	VG_CAP_ROUND },
		{ "square",	VG_CAP_SQUARE },
	};

	for(unsigned i=0; i<COUNTOF(caps); ++i)
		if(strcasecmp(cap, caps[i].name) == 0)
			vgSeti(VG_STROKE_CAP_STYLE,  caps[i].style);
}

void CanvasRenderingContext2D::setLineJoin(const char* join)
{
	struct Join {
		const char* name;
		VGJoinStyle style;
	};

	static const Join joins[] = {
		{ "miter",	VG_JOIN_MITER },
		{ "round",	VG_JOIN_ROUND },
		{ "bevel",	VG_JOIN_BEVEL },
	};

	for(unsigned i=0; i<COUNTOF(joins); ++i)
		if(strcasecmp(join, joins[i].name) == 0)
			vgSeti(VG_STROKE_JOIN_STYLE, joins[i].style);
}

void CanvasRenderingContext2D::setLineWidth(float width)
{
	vgSetf(VG_STROKE_LINE_WIDTH, width);
}

void CanvasRenderingContext2D::setGlobalAlpha(float alpha)
{
	Color sc;
	vgGetParameterfv(openvg->strokePaint, VG_PAINT_COLOR, 4, (float*)&sc);

	Color fc;
	vgGetParameterfv(openvg->fillPaint, VG_PAINT_COLOR, 4, (float*)&fc);

	const float newAlphaFacotr = alpha / _globalAlpha;
	sc.a *= newAlphaFacotr;
	fc.a *= newAlphaFacotr;

	setStrokeColor((float*)&sc);
	setFillColor((float*)&fc);

	_globalAlpha = alpha;
}

ImageData* CanvasRenderingContext2D::createImageData(JSContext* cx, unsigned width, unsigned height)
{
	ImageData* imgData = new ImageData;
	imgData->init(cx, width, height, NULL);

	rhbyte* data = imgData->rawData();
	for(unsigned i=0; i<imgData->length(); ++i)
		data[i] = 0;

	return imgData;
}

ImageData* CanvasRenderingContext2D::createImageData(JSContext* cx, ImageData* imageData)
{
	return createImageData(cx, imageData->width, imageData->height);
}

ImageData* CanvasRenderingContext2D::getImageData(JSContext* cx, unsigned sx, unsigned sy, unsigned sw, unsigned sh)
{
	ImageData* imgData = new ImageData;
	imgData->init(cx, sw, sh, NULL);

	canvas->bindFramebuffer();
	Driver::readPixels(sx, sy, sw, sh, Driver::RGBA, imgData->rawData());

	return imgData;
}

void CanvasRenderingContext2D::putImageData(ImageData* data, unsigned dx, unsigned dy, unsigned dirtyX, unsigned dirtyY, unsigned dirtyWidth, unsigned dirtyHeight)
{
	ASSERT("Not implemented" && dirtyX == 0 && dirtyY == 0);
	ASSERT("Not implemented" && dirtyWidth == data->width && dirtyHeight == data->height);

	canvas->bindFramebuffer();
	Driver::setSamplerState(0, noTexture);

	Driver::writePixels(dx, dy, dirtyWidth, dirtyHeight, Driver::RGBA, data->rawData());
}

void CanvasRenderingContext2D::setFont(const char* font)
{
	_font = font;
}

void CanvasRenderingContext2D::setTextAlign(const char* textAlign)
{
	_textAlign = textAlign;
}

void CanvasRenderingContext2D::setTextBaseLine(const char* textBaseLine)
{
	_textBaseLine = textBaseLine;
}

void CanvasRenderingContext2D::beginBatch()
{
}

void CanvasRenderingContext2D::endBatch()
{
}

}	// namespace Dom
