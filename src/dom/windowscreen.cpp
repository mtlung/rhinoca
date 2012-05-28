#include "pch.h"
#include "windowscreen.h"
#include "window.h"
#include "../context.h"

namespace Dom {

JSClass WindowScreen::jsClass = {
	"Screen", JSCLASS_HAS_PRIVATE | JSCLASS_HAS_RESERVED_SLOTS(1),
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub,  JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSBool assign(JSContext* cx, uintN argc, jsval* vp)
{
	WindowScreen* self = getJsBindable<WindowScreen>(cx, vp);
	Rhinoca* rh = self->window->rhinoca;

	JsString jss(cx, JS_ARGV0);
	if(!jss) return JS_FALSE;

	rh->openDoucment(jss.c_str());
	return JS_TRUE;
}

static JSFunctionSpec methods[] = {
	{"assign", assign, 1,0},
	{"replace", assign, 1,0},
	{0}
};

enum PropertyKey {
	width,
	height,
	availWidth,
	availHeight
};

static JSBool get(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	WindowScreen* self = getJsBindable<WindowScreen>(cx, obj);

	switch(JSID_TO_INT(id)) {
	case width:
	case availWidth:
		*vp = INT_TO_JSVAL(self->width()); return JS_TRUE;
	case height:
	case availHeight:
		*vp = INT_TO_JSVAL(self->height()); return JS_TRUE;
	}

	return JS_TRUE;
}

static JSPropertySpec properties[] = {
	{"width",		width,		JSPROP_READONLY | JsBindable::jsPropFlags, get, JS_StrictPropertyStub},
	{"height",		height,		JSPROP_READONLY | JsBindable::jsPropFlags, get, JS_StrictPropertyStub},
	{"availWidth",	availWidth,	JSPROP_READONLY | JsBindable::jsPropFlags, get, JS_StrictPropertyStub},
	{"availHeight",	availHeight,JSPROP_READONLY | JsBindable::jsPropFlags, get, JS_StrictPropertyStub},
	{0}
};

WindowScreen::WindowScreen(Window* w)
	: window(w)
{
}

void WindowScreen::bind(JSContext* cx, JSObject* parent)
{
	roAssert(!jsContext);
	jsContext = cx;
	jsObject = JS_NewObject(cx, &jsClass, NULL, parent);
	roVerify(JS_SetPrivate(cx, *this, this));
	roVerify(JS_DefineFunctions(cx, *this, methods));
	roVerify(JS_DefineProperties(cx, *this, properties));
	addReference();	// releaseReference() in JsBindable::finalize()

	roVerify(JS_SetReservedSlot(cx, jsObject, 0, *window));
}

unsigned WindowScreen::width() const { return window->width(); }
unsigned WindowScreen::height() const { return window->height(); }
unsigned WindowScreen::availWidth() const { return window->width(); }
unsigned WindowScreen::availHeight() const { return window->height(); }

}	// namespace Dom
