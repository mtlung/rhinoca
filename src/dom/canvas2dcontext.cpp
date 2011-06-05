#include "pch.h"
#include "canvas2dcontext.h"
#include "canvaspixelarray.h"
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
	"CanvasRenderingContext2D", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub, JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSBool getCanvas(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	CanvasRenderingContext2D* self = reinterpret_cast<CanvasRenderingContext2D*>(JS_GetPrivate(cx, obj));
	*vp = *self->canvas;
	return JS_TRUE;
}

static JSBool setGlobalAlpha(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	CanvasRenderingContext2D* self = reinterpret_cast<CanvasRenderingContext2D*>(JS_GetPrivate(cx, obj));
	double a;
	JS_ValueToNumber(cx, *vp, &a);
	self->setGlobalAlpha((float)a);
	return JS_TRUE;
}

static JSBool setStrokeStyle(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	CanvasRenderingContext2D* self = reinterpret_cast<CanvasRenderingContext2D*>(JS_GetPrivate(cx, obj));

	if(JSVAL_IS_OBJECT(*vp)) {
		JSObject* o = JSVAL_TO_OBJECT(*vp);
		CanvasGradient* g = reinterpret_cast<CanvasGradient*>(JS_GetPrivate(cx, o));

		self->setStrokeGradient(g);
	}
	else {
		JSString* jss = JS_ValueToString(cx, *vp);
		char* str = JS_GetStringBytes(jss);

		Color c;
		if(c.parse(str))
			self->setStrokeColor((float*)&c);
		else {
			// TODO: print warning
		}
	}

	return JS_TRUE;
}

static JSBool setFillStyle(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	CanvasRenderingContext2D* self = reinterpret_cast<CanvasRenderingContext2D*>(JS_GetPrivate(cx, obj));

	if(JSVAL_IS_OBJECT(*vp)) {
		JSObject* o = JSVAL_TO_OBJECT(*vp);
		CanvasGradient* g = reinterpret_cast<CanvasGradient*>(JS_GetPrivate(cx, o));

		self->setFillGradient(g);
	}
	else {
		JSString* jss = JS_ValueToString(cx, *vp);
		char* str = JS_GetStringBytes(jss);

		Color c;
		if(c.parse(str))
			self->setFillColor((float*)&c);
		else {
			// TODO: print warning
		}
	}

	return JS_TRUE;
}

static JSBool setLineCap(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	CanvasRenderingContext2D* self = reinterpret_cast<CanvasRenderingContext2D*>(JS_GetPrivate(cx, obj));

	JSString* jss = JS_ValueToString(cx, *vp);
	char* str = JS_GetStringBytes(jss);

	self->setLineCap(str);

	return JS_TRUE;
}

static JSBool setLineJoin(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	CanvasRenderingContext2D* self = reinterpret_cast<CanvasRenderingContext2D*>(JS_GetPrivate(cx, obj));

	JSString* jss = JS_ValueToString(cx, *vp);
	char* str = JS_GetStringBytes(jss);

	self->setLineJoin(str);

	return JS_TRUE;
}

static JSBool setLineWidth(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	CanvasRenderingContext2D* self = reinterpret_cast<CanvasRenderingContext2D*>(JS_GetPrivate(cx, obj));
	double w;
	JS_ValueToNumber(cx, *vp, &w);
	self->setLineWidth((float)w);
	return JS_TRUE;
}

static JSPropertySpec properties[] = {
	{"canvas", 0, 0, getCanvas, JS_PropertyStub},
	{"globalAlpha", 0, 0, JS_PropertyStub, setGlobalAlpha},
	{"strokeStyle", 0, 0, JS_PropertyStub, setStrokeStyle},
	{"lineCap", 0, 0, JS_PropertyStub, setLineCap},
	{"lineJoin", 0, 0, JS_PropertyStub, setLineJoin},
	{"lineWidth", 0, 0, JS_PropertyStub, setLineWidth},
	{"fillStyle", 0, 0, JS_PropertyStub, setFillStyle},
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
	Texture* texture = NULL;
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

static JSBool beginPath(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	if(!JS_InstanceOf(cx, obj, &CanvasRenderingContext2D::jsClass, argv)) return JS_FALSE;
	CanvasRenderingContext2D* self = reinterpret_cast<CanvasRenderingContext2D*>(JS_GetPrivate(cx, obj));
	if(!self) return JS_FALSE;

	self->beginPath();

	return JS_TRUE;
}

static JSBool closePath(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	if(!JS_InstanceOf(cx, obj, &CanvasRenderingContext2D::jsClass, argv)) return JS_FALSE;
	CanvasRenderingContext2D* self = reinterpret_cast<CanvasRenderingContext2D*>(JS_GetPrivate(cx, obj));
	if(!self) return JS_FALSE;

	self->closePath();

	return JS_TRUE;
}

static JSBool moveTo(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	if(!JS_InstanceOf(cx, obj, &CanvasRenderingContext2D::jsClass, argv)) return JS_FALSE;
	CanvasRenderingContext2D* self = reinterpret_cast<CanvasRenderingContext2D*>(JS_GetPrivate(cx, obj));
	if(!self) return JS_FALSE;

	double x, y;
	JS_ValueToNumber(cx, argv[0], &x);
	JS_ValueToNumber(cx, argv[1], &y);
	self->moveTo((float)x, (float)y);

	return JS_TRUE;
}

static JSBool lineTo(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	if(!JS_InstanceOf(cx, obj, &CanvasRenderingContext2D::jsClass, argv)) return JS_FALSE;
	CanvasRenderingContext2D* self = reinterpret_cast<CanvasRenderingContext2D*>(JS_GetPrivate(cx, obj));
	if(!self) return JS_FALSE;

	double x, y;
	JS_ValueToNumber(cx, argv[0], &x);
	JS_ValueToNumber(cx, argv[1], &y);
	self->lineTo((float)x, (float)y);

	return JS_TRUE;
}

static JSBool arc(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	if(!JS_InstanceOf(cx, obj, &CanvasRenderingContext2D::jsClass, argv)) return JS_FALSE;
	CanvasRenderingContext2D* self = reinterpret_cast<CanvasRenderingContext2D*>(JS_GetPrivate(cx, obj));
	if(!self) return JS_FALSE;

	double x, y, radius, startAngle, endAngle;
	JS_ValueToNumber(cx, argv[0], &x);
	JS_ValueToNumber(cx, argv[1], &y);
	JS_ValueToNumber(cx, argv[2], &radius);
	JS_ValueToNumber(cx, argv[3], &startAngle);
	JS_ValueToNumber(cx, argv[4], &endAngle);
	self->arc((float)x, (float)y, (float)radius, (float)startAngle, (float)endAngle, true);

	return JS_TRUE;
}

static JSBool rect(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	if(!JS_InstanceOf(cx, obj, &CanvasRenderingContext2D::jsClass, argv)) return JS_FALSE;
	CanvasRenderingContext2D* self = reinterpret_cast<CanvasRenderingContext2D*>(JS_GetPrivate(cx, obj));
	if(!self) return JS_FALSE;

	double x, y, w, h;
	JS_ValueToNumber(cx, argv[0], &x);
	JS_ValueToNumber(cx, argv[1], &y);
	JS_ValueToNumber(cx, argv[2], &w);
	JS_ValueToNumber(cx, argv[3], &h);
	self->rect((float)x, (float)y, (float)w, (float)h);

	return JS_TRUE;
}

static JSBool createLinearGradient(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	if(!JS_InstanceOf(cx, obj, &CanvasRenderingContext2D::jsClass, argv)) return JS_FALSE;
	CanvasRenderingContext2D* self = reinterpret_cast<CanvasRenderingContext2D*>(JS_GetPrivate(cx, obj));
	if(!self) return JS_FALSE;

	CanvasGradient* g = new CanvasGradient;
	g->bind(cx, NULL);

	double x1, y1, x2, y2;
	JS_ValueToNumber(cx, argv[0], &x1);
	JS_ValueToNumber(cx, argv[1], &y1);
	JS_ValueToNumber(cx, argv[2], &x2);
	JS_ValueToNumber(cx, argv[3], &y2);

	g->createLinear((float)x1, (float)y1, (float)x2, (float)y2);

	*rval = *g;

	return JS_TRUE;
}

static JSBool createRadialGradient(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	if(!JS_InstanceOf(cx, obj, &CanvasRenderingContext2D::jsClass, argv)) return JS_FALSE;
	CanvasRenderingContext2D* self = reinterpret_cast<CanvasRenderingContext2D*>(JS_GetPrivate(cx, obj));
	if(!self) return JS_FALSE;

	CanvasGradient* g = new CanvasGradient;
	g->bind(cx, NULL);

	double x1, y1, r1, x2, y2, r2;
	JS_ValueToNumber(cx, argv[0], &x1);
	JS_ValueToNumber(cx, argv[1], &y1);
	JS_ValueToNumber(cx, argv[2], &r1);
	JS_ValueToNumber(cx, argv[3], &x2);
	JS_ValueToNumber(cx, argv[4], &y2);
	JS_ValueToNumber(cx, argv[5], &r2);

	g->createRadial((float)x1, (float)y1, (float)r1, (float)x2, (float)y2, (float)r2);

	*rval = *g;

	return JS_TRUE;
}

static JSBool stroke(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	if(!JS_InstanceOf(cx, obj, &CanvasRenderingContext2D::jsClass, argv)) return JS_FALSE;
	CanvasRenderingContext2D* self = reinterpret_cast<CanvasRenderingContext2D*>(JS_GetPrivate(cx, obj));
	if(!self) return JS_FALSE;

	self->stroke();

	return JS_TRUE;
}

static JSBool strokeRect(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	if(!JS_InstanceOf(cx, obj, &CanvasRenderingContext2D::jsClass, argv)) return JS_FALSE;
	CanvasRenderingContext2D* self = reinterpret_cast<CanvasRenderingContext2D*>(JS_GetPrivate(cx, obj));
	if(!self) return JS_FALSE;

	double x, y, w, h;
	JS_ValueToNumber(cx, argv[0], &x);
	JS_ValueToNumber(cx, argv[1], &y);
	JS_ValueToNumber(cx, argv[2], &w);
	JS_ValueToNumber(cx, argv[3], &h);
	self->strokeRect((float)x, (float)y, (float)w, (float)h);

	return JS_TRUE;
}

static JSBool fill(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	if(!JS_InstanceOf(cx, obj, &CanvasRenderingContext2D::jsClass, argv)) return JS_FALSE;
	CanvasRenderingContext2D* self = reinterpret_cast<CanvasRenderingContext2D*>(JS_GetPrivate(cx, obj));
	if(!self) return JS_FALSE;

	self->fill();

	return JS_TRUE;
}

static JSBool fillRect(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	if(!JS_InstanceOf(cx, obj, &CanvasRenderingContext2D::jsClass, argv)) return JS_FALSE;
	CanvasRenderingContext2D* self = reinterpret_cast<CanvasRenderingContext2D*>(JS_GetPrivate(cx, obj));
	if(!self) return JS_FALSE;

	double x, y, w, h;
	JS_ValueToNumber(cx, argv[0], &x);
	JS_ValueToNumber(cx, argv[1], &y);
	JS_ValueToNumber(cx, argv[2], &w);
	JS_ValueToNumber(cx, argv[3], &h);
	self->fillRect((float)x, (float)y, (float)w, (float)h);

	return JS_TRUE;
}

static JSBool createImageData(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	if(!JS_InstanceOf(cx, obj, &CanvasRenderingContext2D::jsClass, argv)) return JS_FALSE;
	CanvasRenderingContext2D* self = reinterpret_cast<CanvasRenderingContext2D*>(JS_GetPrivate(cx, obj));
	if(!self) return JS_FALSE;

	JSObject* imgDataParam = NULL;
	ImageData* imgData = NULL;

	if(JS_ValueToObject(cx, argv[0], &imgDataParam) && JS_InstanceOf(cx, imgDataParam, &ImageData::jsClass, argv)) {
		ImageData* imgData = reinterpret_cast<ImageData*>(JS_GetPrivate(cx, imgDataParam));
		imgData = self->createImageData(imgData);
	}
	else if(argc >= 2) {
		int32 sw, sh;
		if(JS_ValueToInt32(cx, argv[0], &sw) && JS_ValueToInt32(cx, argv[1], &sh))
			imgData = self->createImageData(sw, sh);
	}

	if(imgData) {
		imgData->bind(cx, NULL);
		return JS_TRUE;
	}

	return JS_FALSE;
}

static JSBool getImageData(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	if(!JS_InstanceOf(cx, obj, &CanvasRenderingContext2D::jsClass, argv)) return JS_FALSE;
	CanvasRenderingContext2D* self = reinterpret_cast<CanvasRenderingContext2D*>(JS_GetPrivate(cx, obj));
	if(!self) return JS_FALSE;

	int32 x, y, w, h;
	JS_ValueToInt32(cx, argv[0], &x);
	JS_ValueToInt32(cx, argv[1], &y);
	JS_ValueToInt32(cx, argv[2], &w);
	JS_ValueToInt32(cx, argv[3], &h);
	ImageData* imgData = self->getImageData(x, y, w, h);
	imgData->bind(cx, NULL);

	*rval = *imgData;

	return JS_TRUE;
}

static JSBool putImageData(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	if(!JS_InstanceOf(cx, obj, &CanvasRenderingContext2D::jsClass, argv)) return JS_FALSE;
	CanvasRenderingContext2D* self = reinterpret_cast<CanvasRenderingContext2D*>(JS_GetPrivate(cx, obj));
	if(!self) return JS_FALSE;

	JSObject* imgDataParam = NULL;
	if(!JS_ValueToObject(cx, argv[0], &imgDataParam) || !JS_InstanceOf(cx, imgDataParam, &ImageData::jsClass, argv))
		return JS_FALSE;

	ImageData* imgData = reinterpret_cast<ImageData*>(JS_GetPrivate(cx, imgDataParam));

	int32 dx, dy;
	int32 dirtyX = 0, dirtyY = 0;
	int32 dirtyWidth = imgData->width;
	int32 dirtyHeight = imgData->height;

	JS_ValueToInt32(cx, argv[1], &dx);
	JS_ValueToInt32(cx, argv[2], &dy);

	if(argc >= 7) {
		// TODO: Deal with negative values, as state in the spec
		JS_ValueToInt32(cx, argv[3], &dirtyX);
		JS_ValueToInt32(cx, argv[4], &dirtyY);
		JS_ValueToInt32(cx, argv[5], &dirtyWidth);
		JS_ValueToInt32(cx, argv[6], &dirtyHeight);
	}

	self->putImageData(imgData, dx, dy, dirtyX, dirtyY, dirtyWidth, dirtyHeight);

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

	{"beginPath", beginPath, 0,0,0},
	{"closePath", closePath, 0,0,0},
	{"moveTo", moveTo, 2,0,0},
	{"lineTo", lineTo, 2,0,0},
	{"arc", arc, 6,0,0},
	{"rect", rect, 4,0,0},

	{"createLinearGradient", createLinearGradient, 4,0,0},
	{"createRadialGradient", createRadialGradient, 6,0,0},

	{"stroke", stroke, 0,0,0},
	{"strokeRect", strokeRect, 4,0,0},
	{"fill", fill, 0,0,0},
	{"fillRect", fillRect, 4,0,0},

	{"createImageData", createImageData, 1,0,0},
	{"getImageData", getImageData, 4,0,0},
	{"putImageData", putImageData, 3,0,0},

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
		Driver::BlendState::One, Driver::BlendState::One,	// Alpha src, dst
		Driver::BlendState::EnableAll
	};
	Driver::setBlendState(blendState);

	Driver::setViewport(0, 0, w_, h_);
	Driver::ortho(0, w_, 0, h_, 10, -10);
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
		Driver::BlendState::One, Driver::BlendState::One,		// Alpha src, dst
		Driver::BlendState::EnableAll
	};
	Driver::setBlendState(blendState);

	Driver::setViewport(0, 0, w, h);
	Driver::ortho(0, w, 0, h, 10, -10);
	Driver::setViewMatrix(Mat44::identity.data);

	float tw = texture->uvWidth / texture->width;
	float th = texture->uvHeight / texture->height;

	srcx *= tw;	srcw *= tw;
	srcy *= th; srch *= th;

	// Use the texture
	Driver::SamplerState state = {
		texture->handle,
		Driver::SamplerState::MIN_MAG_LINEAR,
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
	startAngle = startAngle * 360 / (2 * 3.14159f);
	endAngle = endAngle * 360 / (2 * 3.14159f);
	radius *= 2;
	vguArc(openvg->path, x,y, radius,radius, startAngle, endAngle-startAngle, VGU_ARC_OPEN);
}

void CanvasRenderingContext2D::rect(float x, float y, float w, float h)
{
	vguRect(openvg->path, x, y, w, h);
	moveTo(x, y);
}

static const Driver::SamplerState noTexture = {
	NULL,
	Driver::SamplerState::MIN_MAG_LINEAR,
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
}

void CanvasRenderingContext2D::isPointInPath(float x, float y)
{
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

void CanvasRenderingContext2D::setFillColor(float* rgba)
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

ImageData* CanvasRenderingContext2D::createImageData(unsigned width, unsigned height)
{
	ImageData* imgData = new ImageData;
	imgData->init(width, height, NULL);

	for(unsigned i=0; i<imgData->data->length; ++i)
		imgData->data->rawData[i] = 0;

	return imgData;
}

ImageData* CanvasRenderingContext2D::createImageData(ImageData* imageData)
{
	return createImageData(imageData->width, imageData->height);
}

ImageData* CanvasRenderingContext2D::getImageData(unsigned sx, unsigned sy, unsigned sw, unsigned sh)
{
	ImageData* imgData = new ImageData;
	imgData->init(width(), height(), NULL);

	Driver::readPixels(sx, sy, sw, sh, Driver::RGBA, imgData->data->rawData);

	return imgData;
}

void CanvasRenderingContext2D::putImageData(ImageData* data, unsigned dx, unsigned dy, unsigned dirtyX, unsigned dirtyY, unsigned dirtyWidth, unsigned dirtyHeight)
{
	ASSERT("Not implemented" && dirtyX == 0 && dirtyY == 0);
	ASSERT("Not implemented" && dirtyWidth == data->width && dirtyHeight == data->height);

	Driver::writePixels(dx, dy, dirtyWidth, dirtyHeight, Driver::RGBA, data->data->rawData);
}

}	// namespace Dom
