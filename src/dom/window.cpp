#include "pch.h"
#include "window.h"
#include "canvas.h"
#include "document.h"
#include "node.h"
#include "windowlocation.h"
#include "../common.h"
#include "../context.h"

extern rhinoca_alertFunc alertFunc;
extern void* alertFuncUserData;

namespace Dom {

JSClass Window::jsClass = {
	"Window", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub, JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSBool alert(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	JSString* jss = JS_ValueToString(cx, argv[0]);
	if(alertFunc && jss) {
		Window* self = reinterpret_cast<Window*>(JS_GetPrivate(cx, obj));
		char* str = JS_GetStringBytes(jss);
		alertFunc(self->rhinoca, alertFuncUserData, str);
		return JS_TRUE;
	}
	return JS_FALSE;
}

static JSBool compileScript(TimerCallback* cb, JSContext* cx, JSObject* obj, jsval* argv)
{
	JSString* jss = JS_ValueToString(cx, argv[0]);
	cb->jsScript = JS_CompileScript(cx, obj, JS_GetStringBytes(jss), JS_GetStringLength(jss), NULL, 0);
	if(!cb->jsScript) return JS_FALSE;
	cb->jsScriptObject = JS_NewScriptObject(cx, cb->jsScript);
	if(!cb->jsScriptObject) { JS_DestroyScript(cx, cb->jsScript); return JS_FALSE; }
	VERIFY(JS_AddNamedRoot(cx, &cb->jsScriptObject, "TimerCallback's script object"));

	return JS_TRUE;
}

JSBool setTimeout(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	int32 timeout;	// In unit of millisecond
	if(!JS_ValueToInt32(cx, argv[1], &timeout)) return JS_FALSE;
	Window* self = reinterpret_cast<Window*>(JS_GetPrivate(cx, obj));

	TimerCallback* cb = new TimerCallback;
	cb->bind(cx, NULL);

	if(JSVAL_IS_STRING(argv[0])) {
		if(!compileScript(cb, cx, obj, argv)) return JS_FALSE;
	}
	else {
		cb->closure = argv[0];
		VERIFY(JS_AddNamedRoot(cx, &cb->closure, "Closure of TimerCallback"));
	}

	cb->interval = 0;
	cb->setNextInvokeTime(float(self->timer.seconds()) + (float)timeout/1000);

	self->timerCallbacks.insert(*cb);
	cb->addGcRoot();	// releaseGcRoot() in ~TimerCallback::removeThis()

	*rval = *cb;

	return JS_TRUE;
}

JSBool setInterval(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	int32 interval;	// In unit of millisecond
	if(!JS_ValueToInt32(cx, argv[1], &interval)) return JS_FALSE;
	Window* self = reinterpret_cast<Window*>(JS_GetPrivate(cx, obj));

	TimerCallback* cb = new TimerCallback;
	cb->bind(cx, NULL);

	if(JSVAL_IS_STRING(argv[0])) {
		if(!compileScript(cb, cx, obj, argv)) return JS_FALSE;
	}
	else {
		cb->closure = argv[0];
		VERIFY(JS_AddNamedRoot(cx, &cb->closure, "Closure of TimerCallback"));
	}

	cb->interval = (float)interval/1000;
	cb->setNextInvokeTime(float(self->timer.seconds()) + cb->interval);

	self->timerCallbacks.insert(*cb);
	cb->addGcRoot();	// releaseGcRoot() in TimerCallback::removeThis()

	*rval = *cb;

	return JS_TRUE;
}

JSBool clearTimeout(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	JSObject* jsTimerCallback = NULL;
	VERIFY(JS_ValueToObject(cx, argv[0], &jsTimerCallback));
	if(!JS_InstanceOf(cx, jsTimerCallback, &TimerCallback::jsClass, argv)) return JS_FALSE;

	TimerCallback* timerCallback = reinterpret_cast<TimerCallback*>(JS_GetPrivate(cx, jsTimerCallback));

	if(timerCallback->isInMap())
		timerCallback->removeThis();

	return JS_TRUE;
}

JSBool clearInterval(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	JSObject* jsTimerCallback = NULL;
	VERIFY(JS_ValueToObject(cx, argv[0], &jsTimerCallback));
	if(!JS_InstanceOf(cx, jsTimerCallback, &TimerCallback::jsClass, argv)) return JS_FALSE;

	TimerCallback* cb = reinterpret_cast<TimerCallback*>(JS_GetPrivate(cx, jsTimerCallback));

	if(cb->isInMap())
		cb->removeThis();

	return JS_TRUE;
}

// NOTE: For simplicity, we ignore the second parameter to requestAnimationFrame
JSBool requestAnimationFrame(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	Window* self = reinterpret_cast<Window*>(JS_GetPrivate(cx, obj));

	FrameRequestCallback* cb = new FrameRequestCallback;
	cb->bind(cx, NULL);

	if(!JSVAL_IS_OBJECT(argv[0]))
		return JS_FALSE;

	cb->closure = argv[0];
	VERIFY(JS_AddNamedRoot(cx, &cb->closure, "Closure of FrameRequestCallback"));

	self->frameRequestCallbacks.pushFront(*cb);

	cb->addGcRoot();	// releaseGcRoot() in FrameRequestCallback::removeThis()

	*rval = *cb;

	return JS_TRUE;
}

JSBool cancelRequestAnimationFrame(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	JSObject* jsAnimCallback = NULL;
	VERIFY(JS_ValueToObject(cx, argv[0], &jsAnimCallback));
	if(!JS_InstanceOf(cx, jsAnimCallback, &FrameRequestCallback::jsClass, argv)) return JS_FALSE;

	FrameRequestCallback* cb = reinterpret_cast<FrameRequestCallback*>(JS_GetPrivate(cx, jsAnimCallback));

	if(cb->isInList())
		cb->removeThis();

	return JS_TRUE;
}

static JSFunctionSpec methods[] = {
	{"alert", alert, 1,0,0},
	{"setTimeout", setTimeout, 2,0,0},
	{"setInterval", setInterval, 2,0,0},
	{"clearTimeout", clearTimeout, 1,0,0},
	{"clearInterval", clearInterval, 1,0,0},
	{"requestAnimationFrame", requestAnimationFrame, 2,0,0},
	{"cancelRequestAnimationFrame", cancelRequestAnimationFrame, 1,0,0},
	{0}
};

static JSBool location(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	Window* self = reinterpret_cast<Window*>(JS_GetPrivate(cx, obj));
	WindowLocation* wLoc = new WindowLocation(self);
	wLoc->bind(cx, NULL);
	*vp = *wLoc;
	return JS_TRUE;
}

static JSPropertySpec properties[] = {
	{"location", 0, JSPROP_READONLY, location, JS_PropertyStub},
	{0}
};

Window::Window(Rhinoca* rh)
	: rhinoca(rh)
{
	document = new HTMLDocument(rh);
	virtualCanvas = new HTMLCanvasElement();
	virtualCanvas->useExternalFrameBuffer(rh);
	virtualCanvas->createContext("2d");
}

Window::~Window()
{
	document->releaseGcRoot();

	// Explicit call node's removeThis() since it's not a virtual function
	while(TimerCallback* cb = timerCallbacks.findMin())
		cb->removeThis();

	while(!frameRequestCallbacks.isEmpty())
		frameRequestCallbacks.begin()->removeThis();

	delete virtualCanvas;

	JS_GC(jsContext);
}

void Window::bind(JSContext* cx, JSObject* parent)
{
	ASSERT(!jsContext);
	jsContext = cx;
	jsObject = JS_DefineObject(cx, parent, "window", &jsClass, 0, JSPROP_ENUMERATE);
	VERIFY(JS_SetPrivate(cx, *this, this));
	VERIFY(JS_DefineFunctions(cx, *this, methods));
	VERIFY(JS_DefineProperties(cx, *this, properties));
	addReference();

	document->bind(cx, parent);
	document->addGcRoot();	// releaseGcRoot() in ~Window()
}

void Window::update()
{
	rhuint64 usSince1970 = Timer::microSecondsSince1970();

	// Invoke animation request callbacks
	for(FrameRequestCallback* cb = frameRequestCallbacks.begin(); cb != frameRequestCallbacks.end();)
	{
		jsval rval;
		ASSERT(cb->closure);
		jsval argv;
		VERIFY(JS_NewNumberValue(jsContext, double(usSince1970 / 1000), &argv));
		JS_CallFunctionValue(jsContext, *this, cb->closure, 1, &argv, &rval);
		FrameRequestCallback* bk = cb;
		cb = cb->next();
		bk->removeThis();
	}

	float now = (float)timer.seconds();

	// Invoke timer callbacks
	while(TimerCallback* cb = timerCallbacks.findMin())
	{
		if(now < cb->nextInvokeTime())
			break;

		jsval rval;
		if(cb->jsScript)
			JS_ExecuteScript(jsContext, *this, cb->jsScript, &rval);
		else
			JS_CallFunctionValue(jsContext, *this, cb->closure, 0, NULL, &rval);

		if(cb->interval > 0) {
			float t = cb->nextInvokeTime();
			while(t <= now)
				t += cb->interval;
			cb->setNextInvokeTime(t);
		}
		else
			cb->removeThis();
	}
}

void Window::render()
{
	if(!document) return;

	// Resize the virtual canvas if needed
	if(virtualCanvas->width() != width())
		virtualCanvas->setWidth(width());
	if(virtualCanvas->height() != height())
		virtualCanvas->setHeight(height());

	for(NodeIterator i(document); !i.ended(); i.next()) {
		i->render();
	}
}

unsigned Window::width() const
{
	return rhinoca->width;
}

unsigned Window::height() const
{
	return rhinoca->height;
}

JSClass TimerCallback::jsClass = {
	"TimerCallback", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub, JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
};

TimerCallback::TimerCallback()
	: closure(NULL), jsScript(NULL), jsScriptObject(NULL)
{
}

TimerCallback::~TimerCallback()
{
	ASSERT(closure == 0);
	ASSERT(jsScript == NULL);
}

void TimerCallback::bind(JSContext* cx, JSObject* parent)
{
	ASSERT(!jsContext);
	jsContext = cx;
	jsObject = JS_NewObject(cx, &jsClass, NULL, parent);
	VERIFY(JS_SetPrivate(cx, *this, this));
	addReference();
}

void TimerCallback::removeThis()
{
	super::removeThis();
	closure = NULL;
	jsScript = NULL;
	jsScriptObject = NULL;
	VERIFY(JS_RemoveRoot(jsContext, &closure));
	VERIFY(JS_RemoveRoot(jsContext, &jsScriptObject));
	releaseGcRoot();
}

JSClass FrameRequestCallback::jsClass = {
	"FrameRequestCallback", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub, JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
};

FrameRequestCallback::FrameRequestCallback()
	: closure(NULL)
{
}

FrameRequestCallback::~FrameRequestCallback()
{
	ASSERT(closure == 0);
}

void FrameRequestCallback::bind(JSContext* cx, JSObject* parent)
{
	ASSERT(!jsContext);
	jsContext = cx;
	jsObject = JS_NewObject(cx, &jsClass, NULL, parent);
	VERIFY(JS_SetPrivate(cx, *this, this));
	addReference();
}

void FrameRequestCallback::removeThis()
{
	super::removeThis();
	closure = NULL;
	VERIFY(JS_RemoveRoot(jsContext, &closure));
	releaseGcRoot();
}

}	// namespace Dom
