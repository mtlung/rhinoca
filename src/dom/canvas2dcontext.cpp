#include "pch.h"
#include "canvas2dcontext.h"
#include "canvasgradient.h"
#include "color.h"
#include "image.h"
#include "imagedata.h"
#include "../../roar/base/roLog.h"

using namespace Render;
using namespace ro;

namespace Dom {

JSClass CanvasRenderingContext2D::jsClass = {
	"CanvasRenderingContext2D", JSCLASS_HAS_PRIVATE | JSCLASS_HAS_RESERVED_SLOTS(1),
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub, JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
};

static void getFloat(JSContext* cx, jsval* vp, float* dest, unsigned count)
{
	for(unsigned i=0; i<count; ++i) {
		double tmp;
		roVerify(JS_ValueToNumber(cx, JS_ARGV(cx, vp)[i], &tmp));
		dest[i] = (float)tmp;
	}
}

static void getFloat(JSContext* cx, jsval* vp, unsigned argOffset, float* dest, unsigned count)
{
	for(unsigned i=0; i<count; ++i) {
		double tmp;
		roVerify(JS_ValueToNumber(cx, JS_ARGV(cx, vp)[i + argOffset], &tmp));
		dest[i] = (float)tmp;
	}
}

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
	self->_canvas.setGlobalAlpha((float)a);
	return JS_TRUE;
}

static JSBool getStrokeStyle(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, obj);
	if(!self) return JS_FALSE;

	Color c;
	self->_canvas.getStrokeColor((float*)(&c));

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

		self->_canvas.setStrokeGradient(g->handle);
	}
	else {
		Color c;
		JsString jss(cx, *vp);
		if(c.parse(jss.c_str()))
			self->_canvas.setStrokeColor((float*)&c);
		else {
			roLog("warn", "Fail to parse \"%s\" as a color\n", jss.c_str());
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
	self->_canvas.getFillColor((float*)(&c));

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

		self->_canvas.setFillGradient(g->handle);
	}
	else {
		Color c;
		JsString jss(cx, *vp);
		if(c.parse(jss.c_str()))
			self->_canvas.setFillColor((float*)&c);
		else {
			roLog("warn", "Fail to parse \"%s\" as a color\n", jss.c_str());
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
	self->_canvas.setLineCap(jss.c_str());

	return JS_TRUE;
}

static JSBool setLineJoin(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, obj);
	if(!self) return JS_FALSE;

	JsString jss(cx, *vp);
	self->_canvas.setLineJoin(jss.c_str());

	return JS_TRUE;
}

static JSBool setLineWidth(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, obj);
	if(!self) return JS_FALSE;

	double w;
	JS_ValueToNumber(cx, *vp, &w);
	self->_canvas.setLineWidth((float)w);
	return JS_TRUE;
}

static JSBool getFont(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, obj);
	if(!self) return JS_FALSE;

	roAssert(false);
	*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, self->_canvas.font()));
	return JS_TRUE;
}

static JSBool setFont(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, obj);
	if(!self) return JS_FALSE;

	JsString jss(cx, *vp);
	if(!jss) return JS_FALSE;

	self->_canvas.setFont(jss.c_str());
	return JS_TRUE;
}

static JSBool getTextAlign(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, obj);
	if(!self) return JS_FALSE;

	roAssert(false);
	*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, self->_canvas.textAlign()));
	return JS_TRUE;
}

static JSBool setTextAlign(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, obj);
	if(!self) return JS_FALSE;

	JsString jss(cx, *vp);
	if(!jss) return JS_FALSE;

	self->_canvas.setTextAlign(jss.c_str());
	return JS_TRUE;
}

static JSBool getTextBaseLine(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, obj);
	if(!self) return JS_FALSE;

	*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, self->_canvas.textBaseline()));
	return JS_TRUE;
}

static JSBool setTextBaseLine(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, obj);
	if(!self) return JS_FALSE;

	JsString jss(cx, *vp);
	if(!jss) return JS_FALSE;

	self->_canvas.setTextBaseline(jss.c_str());
	return JS_TRUE;
}

static JSBool setUseImgDimension(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, obj);
	if(!self) return JS_FALSE;

	JSBool b;
	if(!JS_ValueToBoolean(cx, *vp, &b))
		return JS_FALSE;

	roAssert(false);
//	self->useImgDimension = b;
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

	struct { float x, y, w, h; } s;
	getFloat(cx, vp, &s.x, 4);

	self->_canvas.clearRect(s.x, s.y, s.w, s.h);

	return JS_TRUE;
}

static JSBool beginLayer(JSContext* cx, uintN argc, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, vp);
	if(!self) return JS_FALSE;

	self->_canvas.beginDrawImageBatch();
	return JS_TRUE;
}

static JSBool endLayer(JSContext* cx, uintN argc, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, vp);
	if(!self) return JS_FALSE;

	self->_canvas.endDrawImageBatch();
	return JS_TRUE;
}

static JSBool drawImage(JSContext* cx, uintN argc, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, vp);
	if(!self) return JS_FALSE;

	// Determine the source is an image or a canvas
	ro::Texture* texture = NULL;

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
//		filter = Driver::SamplerState::MIN_MAG_LINEAR;
		imgw = otherCanvas->width();
		imgh = otherCanvas->height();
	}

	if(!texture) return JS_FALSE;

//	if(!self->useImgDimension) {
//		imgw = texture->virtualWidth;
//		imgh = texture->virtualHeight;
//	}

	const float scalex = 1;//float(texture->virtualWidth) / imgw;
	const float scaley = 1;//float(texture->virtualHeight) / imgh;

	struct {
		float sx, sy, sw, sh;	// Source x, y, width and height
		float dx, dy, dw, dh;	// Dest x, y, width and height
	} s;

	switch(argc) {
	case 3:
		s.sx = s.sy = 0;
		s.sw = (float)texture->width();
		s.sh = (float)texture->height();
//		s.sw = (float)texture->virtualWidth;
//		s.sh = (float)texture->virtualHeight;
		s.dw = (float)imgw;
		s.dh = (float)imgh;
		getFloat(cx, vp, 1, &s.dx, argc-1);
		break;
	case 5:
		s.sx = s.sy = 0;
		s.sw = (float)texture->width();
		s.sh = (float)texture->height();
//		s.sw = (float)texture->virtualWidth;
//		s.sh = (float)texture->virtualHeight;
		getFloat(cx, vp, 1, &s.dx, argc-1);
		break;
	case 9:
		getFloat(cx, vp, 1, &s.sx, argc-1);

		s.sx *= scalex;
		s.sy *= scaley;
		s.sw *= scalex;
		s.sh *= scaley;
		break;
	default:
		return JS_FALSE;
	}

	self->_canvas.drawImage(
		texture->handle,
		s.sx, s.sy, s.sw, s.sh,
		s.dx, s.dy, s.dw, s.dh
	);

	return JS_TRUE;
}

static JSBool save(JSContext* cx, uintN argc, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, vp);
	if(!self) return JS_FALSE;

	self->_canvas.save();

	return JS_TRUE;
}

static JSBool restore(JSContext* cx, uintN argc, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, vp);
	if(!self) return JS_FALSE;

	self->_canvas.restore();

	return JS_TRUE;
}

static JSBool scale(JSContext* cx, uintN argc, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, vp);
	if(!self) return JS_FALSE;

	struct { float x, y; } s;
	getFloat(cx, vp, &s.x, 2);
	self->_canvas.scale(s.x, s.y);

	return JS_TRUE;
}

static JSBool rotate(JSContext* cx, uintN argc, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, vp);
	if(!self) return JS_FALSE;

	double angle;
	roVerify(JS_ValueToNumber(cx, JS_ARGV0, &angle));
	self->_canvas.rotate((float)angle);

	return JS_TRUE;
}

static JSBool translate(JSContext* cx, uintN argc, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, vp);
	if(!self) return JS_FALSE;

	struct { float x, y; } s;
	getFloat(cx, vp, &s.x, 2);
	self->_canvas.translate(s.x, s.y);

	return JS_TRUE;
}

static JSBool transform(JSContext* cx, uintN argc, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, vp);
	if(!self) return JS_FALSE;

	struct { float m11, m12, m21, m22, dx, dy; } s;
	getFloat(cx, vp, &s.m11, 6);

	self->_canvas.transform(s.m11, s.m21, s.m21, s.m22, s.dx, s.dy);

	return JS_TRUE;
}

static JSBool setTransform(JSContext* cx, uintN argc, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, vp);
	if(!self) return JS_FALSE;

	struct { float m11, m12, m21, m22, dx, dy; } s;
	getFloat(cx, vp, &s.m11, 6);

	self->_canvas.setTransform(s.m11, s.m21, s.m21, s.m22, s.dx, s.dy);

	return JS_TRUE;
}

static JSBool beginPath(JSContext* cx, uintN argc, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, vp);
	if(!self) return JS_FALSE;

	self->_canvas.beginPath();

	return JS_TRUE;
}

static JSBool closePath(JSContext* cx, uintN argc, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, vp);
	if(!self) return JS_FALSE;

	self->_canvas.closePath();

	return JS_TRUE;
}

static JSBool moveTo(JSContext* cx, uintN argc, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, vp);
	if(!self) return JS_FALSE;

	struct { float x, y; } s;
	getFloat(cx, vp, &s.x, 2);
	self->_canvas.moveTo(s.x, s.y);

	return JS_TRUE;
}

static JSBool lineTo(JSContext* cx, uintN argc, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, vp);
	if(!self) return JS_FALSE;

	struct { float x, y; } s;
	getFloat(cx, vp, &s.x, 2);
	self->_canvas.lineTo(s.x, s.y);

	return JS_TRUE;
}

static JSBool quadraticCurveTo(JSContext* cx, uintN argc, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, vp);
	if(!self) return JS_FALSE;

	struct { float cpx, cpy, x, y; } s;
	getFloat(cx, vp, &s.cpx, 4);
	self->_canvas.quadraticCurveTo(s.cpx, s.cpy, s.x, s.y);

	return JS_TRUE;
}

static JSBool bezierCurveTo(JSContext* cx, uintN argc, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, vp);
	if(!self) return JS_FALSE;

	struct { float cp1x, cp1y, cp2x, cp2y, x, y; } s;
	getFloat(cx, vp, &s.cp1x, 6);
	self->_canvas.bezierCurveTo(s.cp1x, s.cp1y, s.cp2x, s.cp2y, s.x, s.y);

	return JS_TRUE;
}

static JSBool arc(JSContext* cx, uintN argc, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, vp);
	if(!self) return JS_FALSE;

	bool antiClockwise;
	struct { float x, y, radius, startAngle, endAngle; } s;
	getFloat(cx, vp, &s.x, 5);
	roVerify(JS_GetValue(cx, JS_ARGV5, antiClockwise));
	self->_canvas.arc(s.x, s.y, s.radius, s.startAngle, s.endAngle, antiClockwise);

	return JS_TRUE;
}

static JSBool rect(JSContext* cx, uintN argc, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, vp);
	if(!self) return JS_FALSE;

	struct { float x, y, w, h; } s;
	getFloat(cx, vp, &s.x, 4);
	self->_canvas.rect(s.x, s.y, s.w, s.h);

	return JS_TRUE;
}

static JSBool createLinearGradient(JSContext* cx, uintN argc, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, vp);
	if(!self) return JS_FALSE;

	CanvasGradient* g = new CanvasGradient;
	g->bind(cx, NULL);

	struct { float x1, y1, x2, y2; } s;
	getFloat(cx, vp, &s.x1, 4);
	g->createLinear(s.x1, s.y1, s.x2, s.y2);

	JS_RVAL(cx, vp) = *g;

	return JS_TRUE;
}

static JSBool createRadialGradient(JSContext* cx, uintN argc, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, vp);
	if(!self) return JS_FALSE;

	CanvasGradient* g = new CanvasGradient;
	g->bind(cx, NULL);

	struct { float x1, y1, r1, x2, y2, r2; } s;
	getFloat(cx, vp, &s.x1, 6);
	g->createRadial(s.x1, s.y1, s.r1, s.x2, s.y2, s.r2);

	JS_RVAL(cx, vp) = *g;

	return JS_TRUE;
}

static JSBool stroke(JSContext* cx, uintN argc, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, vp);
	if(!self) return JS_FALSE;

	self->_canvas.stroke();

	return JS_TRUE;
}

static JSBool strokeRect(JSContext* cx, uintN argc, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, vp);
	if(!self) return JS_FALSE;

	struct { float x, y, w, h; } s;
	getFloat(cx, vp, &s.x, 4);
	self->_canvas.strokeRect(s.x, s.y, s.w, s.h);

	return JS_TRUE;
}

static JSBool fill(JSContext* cx, uintN argc, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, vp);
	if(!self) return JS_FALSE;

	self->_canvas.fill();

	return JS_TRUE;
}

static JSBool fillRect(JSContext* cx, uintN argc, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, vp);
	if(!self) return JS_FALSE;

	struct { float x, y, w, h; } s;
	getFloat(cx, vp, &s.x, 4);
	self->_canvas.fillRect(s.x, s.y, s.w, s.h);

	return JS_TRUE;
}

static JSBool createImageData(JSContext* cx, uintN argc, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, vp);
	if(!self) return JS_FALSE;

	ImageData* imgData = getJsBindable<ImageData>(cx, vp, 0);

/*	if(imgData)
		imgData = self->createImageData(cx, imgData);
	else if(argc >= 2) {
		int32 sw, sh;
		if(JS_ValueToInt32(cx, JS_ARGV0, &sw) && JS_ValueToInt32(cx, JS_ARGV1, &sh))
			imgData = self->createImageData(cx, sw, sh);
	}*/

	return imgData ? JS_TRUE : JS_FALSE;
}

static JSBool getImageData(JSContext* cx, uintN argc, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, vp);
	if(!self) return JS_FALSE;

	int32 x, y, w, h;
	roVerify(JS_ValueToInt32(cx, JS_ARGV0, &x));
	roVerify(JS_ValueToInt32(cx, JS_ARGV1, &y));
	roVerify(JS_ValueToInt32(cx, JS_ARGV2, &w));
	roVerify(JS_ValueToInt32(cx, JS_ARGV3, &h));
//	ImageData* imgData = self->getImageData(cx, x, y, w, h);

//	JS_RVAL(cx, vp) = *imgData;

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

	roVerify(JS_ValueToInt32(cx, JS_ARGV1, &dx));
	roVerify(JS_ValueToInt32(cx, JS_ARGV2, &dy));

	if(argc >= 7) {
		// TODO: Deal with negative values, as state in the spec
		roVerify(JS_ValueToInt32(cx, JS_ARGV3, &dirtyX));
		roVerify(JS_ValueToInt32(cx, JS_ARGV4, &dirtyY));
		roVerify(JS_ValueToInt32(cx, JS_ARGV5, &dirtyWidth));
		roVerify(JS_ValueToInt32(cx, JS_ARGV6, &dirtyHeight));
	}

//	self->putImageData(imgData, dx, dy, dirtyX, dirtyY, dirtyWidth, dirtyHeight);

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

	self->_canvas.beginDrawImageBatch();

	return JS_TRUE;
}

static JSBool endBatch(JSContext* cx, uintN argc, jsval* vp)
{
	CanvasRenderingContext2D* self = getJsBindable<CanvasRenderingContext2D>(cx, vp);
	if(!self) return JS_FALSE;

	self->_canvas.endDrawImageBatch();

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


CanvasRenderingContext2D::CanvasRenderingContext2D(HTMLCanvasElement* c)
	: Context(c)
{
	_canvas.init();
}

CanvasRenderingContext2D::~CanvasRenderingContext2D()
{
	_canvas.destroy();
}

void CanvasRenderingContext2D::bind(JSContext* cx, JSObject* parent)
{
	roAssert(!jsContext);
	jsContext = cx;
	jsObject = JS_NewObject(cx, &jsClass, NULL, parent);
	roVerify(JS_SetPrivate(cx, *this, this));
	roVerify(JS_DefineFunctions(cx, *this, methods));
	roVerify(JS_DefineProperties(cx, *this, properties));
	addReference();	// releaseReference() in JsBindable::finalize()
}

static JSBool construct(JSContext* cx, uintN argc, jsval* vp)
{
	roAssert(false && "For compatible with javascript instanceof operator only, you are not suppose to new a CanvasRenderingContext2D directly");
	return JS_FALSE;
}

void CanvasRenderingContext2D::registerClass(JSContext* cx, JSObject* parent)
{
	roVerify(JS_InitClass(cx, parent, NULL, &jsClass, &construct, 0, NULL, NULL, NULL, NULL));
}

void CanvasRenderingContext2D::setWidth(unsigned w)
{
	if(w == width()) return;
	if(_canvas.targetTexture)
		_canvas.initTargetTexture(w, height());
}

void CanvasRenderingContext2D::setHeight(unsigned h)
{
	if(h == height()) return;
	if(_canvas.targetTexture)
		_canvas.initTargetTexture(width(), h);
}

ro::Texture* CanvasRenderingContext2D::texture()
{
	return _canvas.targetTexture.get();
}

}	// namespace Dom
