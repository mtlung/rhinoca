#include "pch.h"
#include "canvas2dcontext.h"
#include "image.h"
#include "../render/glew.h"

using namespace Render;

namespace Dom {

JSClass Canvas2dContext::jsClass = {
	"Canvas2dContext", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub, JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSBool clearRect(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	if(!JS_InstanceOf(cx, obj, &Canvas2dContext::jsClass, argv)) return JS_FALSE;
	Canvas2dContext* self = reinterpret_cast<Canvas2dContext*>(JS_GetPrivate(cx, obj));
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
	if(!JS_InstanceOf(cx, obj, &Canvas2dContext::jsClass, argv)) return JS_FALSE;
	Canvas2dContext* self = reinterpret_cast<Canvas2dContext*>(JS_GetPrivate(cx, obj));
	if(!self) return JS_FALSE;
	self->beginLayer();
	return JS_TRUE;
}

static JSBool endLayer(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	if(!JS_InstanceOf(cx, obj, &Canvas2dContext::jsClass, argv)) return JS_FALSE;
	Canvas2dContext* self = reinterpret_cast<Canvas2dContext*>(JS_GetPrivate(cx, obj));
	if(!self) return JS_FALSE;
	self->endLayer();
	return JS_TRUE;
}

static JSBool drawImage(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	if(!JS_InstanceOf(cx, obj, &Canvas2dContext::jsClass, argv)) return JS_FALSE;
	Canvas2dContext* self = reinterpret_cast<Canvas2dContext*>(JS_GetPrivate(cx, obj));
	if(!self) return JS_FALSE;

	JSObject* imgObj = NULL;
	JS_ValueToObject(cx, argv[0], &imgObj);

	// Determine the source is an image or a canvas
	Texture* texture;
	if(JS_InstanceOf(cx, imgObj, &Image::jsClass, NULL)) {
		Image* img = reinterpret_cast<Image*>(JS_GetPrivate(cx ,imgObj));
		texture = img->texture.get();
	}
	if(JS_InstanceOf(cx, imgObj, &Canvas::jsClass, NULL)) {
		Canvas* otherCanvas = reinterpret_cast<Canvas*>(JS_GetPrivate(cx ,imgObj));
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

static JSFunctionSpec methods[] = {
	{"clearRect", clearRect, 4,0,0},
	{"beginLayer", beginLayer, 0,0,0},
	{"endLayer", endLayer, 0,0,0},
	{"drawImage", drawImage, 5,0,0},
	{0}
};

Canvas2dContext::Canvas2dContext(Canvas* c)
	: Context(c)
{
}

Canvas2dContext::~Canvas2dContext()
{
	releaseGcRoot();
}

void Canvas2dContext::bind(JSContext* cx, JSObject* parent)
{
	ASSERT(!jsContext);
	jsContext = cx;
	jsObject = JS_NewObject(cx, &jsClass, NULL, NULL);
	VERIFY(JS_SetPrivate(cx, jsObject, this));
	VERIFY(JS_DefineFunctions(cx, jsObject, methods));
	addReference();
}

void Canvas2dContext::beginLayer()
{
}

void Canvas2dContext::endLayer()
{
}

void Canvas2dContext::clearRect(float x, float y, float w, float h)
{
}

void Canvas2dContext::drawImage(
	Texture* texture,
	float dstx, float dsty)
{
	drawImage(
		texture,
		0, 0, (float)texture->width, (float)texture->height,
		dstx, dsty, (float)texture->width, (float)texture->height
	);
}

void Canvas2dContext::drawImage(
	Texture* texture,
	float dstx, float dsty, float dstw, float dsth)
{
	drawImage(texture,
		0, 0, (float)texture->width, (float)texture->height,
		dstx, dsty, dstw, dsth
	);
}

void Canvas2dContext::drawImage(
	Texture* texture,
	float srcx, float srcy, float srcw, float srch,
	float dstx, float dsty, float dstw, float dsth)
{
	canvas->bindFramebuffer();

	unsigned w = width();
	unsigned h = height();

	glViewport(0, 0, w, h);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, w, 0, h, 10, -10);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	float tw = 1.0f / texture->width;
	float th = 1.0f / texture->height;

	srcx *= tw;	srcw *= tw;
	srcy *= th; srch *= th;

	glEnable(GL_TEXTURE_2D);
	texture->bind();

	glBegin(GL_QUADS);
		glTexCoord2f(srcx +    0, srcy +    0);
		glVertex3f(  dstx +    0, dsty +    0, 1);

		glTexCoord2f(srcx + srcw, srcy +    0);
		glVertex3f(  dstx + dstw, dsty +    0, 1);

		glTexCoord2f(srcx + srcw, srcy + srch);
		glVertex3f(  dstx + dstw, dsty + dsth, 1);

		glTexCoord2f(srcx +    0, srcy + srch);
		glVertex3f(  dstx +    0, dsty + dsth, 1);
	glEnd();

	canvas->unbindFramebuffer();
}

void Canvas2dContext::save()
{
}

void Canvas2dContext::restore()
{
}

void Canvas2dContext::scale(float x, float y)
{
}

void Canvas2dContext::rotate(float angle)
{
}

void Canvas2dContext::translate(float x, float y)
{
}

void Canvas2dContext::transform(float m11, float m12, float m22, float dx, float dy)
{
}

void Canvas2dContext::setTransform(float m11, float m12, float m22, float dx, float dy)
{
}

}	// namespace Dom
