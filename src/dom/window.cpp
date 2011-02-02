#include "pch.h"
#include "window.h"
#include "canvas.h"
#include "document.h"
#include "node.h"
#include "../common.h"
#include "../context.h"

extern rhinoca_alertFunc alertFunc;
extern void* alertFuncUserData;

namespace Dom {

JSClass DOMWindow::jsClass = {
	"DOMWindow", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub, JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSBool alert(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	JSString* jss = JS_ValueToString(cx, argv[0]);
	if(alertFunc && jss) {
		DOMWindow* self = reinterpret_cast<DOMWindow*>(JS_GetPrivate(cx, obj));
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
	DOMWindow* self = reinterpret_cast<DOMWindow*>(JS_GetPrivate(cx, obj));

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

	*rval = OBJECT_TO_JSVAL(cb->jsObject);

	return JS_TRUE;
}

JSBool setInterval(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	int32 interval;	// In unit of millisecond
	if(!JS_ValueToInt32(cx, argv[1], &interval)) return JS_FALSE;
	DOMWindow* self = reinterpret_cast<DOMWindow*>(JS_GetPrivate(cx, obj));

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
	cb->addGcRoot();	// releaseGcRoot() in ~TimerCallback::removeThis()

	*rval = OBJECT_TO_JSVAL(cb->jsObject);

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

	TimerCallback* timerCallback = reinterpret_cast<TimerCallback*>(JS_GetPrivate(cx, jsTimerCallback));

	if(timerCallback->isInMap())
		timerCallback->removeThis();

	return JS_TRUE;
}

static JSFunctionSpec methods[] = {
	{"alert", alert, 1,0,0},
	{"setTimeout", setTimeout, 2,0,0},
	{"setInterval", setInterval, 2,0,0},
	{"clearTimeout", clearTimeout, 1,0,0},
	{"clearInterval", clearInterval, 1,0,0},
	{0}
};

DOMWindow::DOMWindow(Rhinoca* rh)
	: rhinoca(rh)
{
	document = new HTMLDocument(rh);
	virtualCanvas = new HTMLCanvasElement();
	virtualCanvas->useExternalFrameBuffer(rh);
	virtualCanvas->createContext("2d");
}

DOMWindow::~DOMWindow()
{
	document->releaseGcRoot();

	while(TimerCallback* cb = timerCallbacks.findMin())
		cb->removeThis();

	delete virtualCanvas;

	JS_GC(jsContext);
}

void DOMWindow::bind(JSContext* cx, JSObject* parent)
{
	ASSERT(!jsContext);
	jsContext = cx;
	jsObject = JS_DefineObject(cx, parent, "window", &jsClass, 0, JSPROP_ENUMERATE);
	VERIFY(JS_SetPrivate(cx, jsObject, this));
	VERIFY(JS_DefineFunctions(cx, jsObject, methods));
	addReference();

	document->bind(cx, parent);
	document->addGcRoot();	// releaseGcRoot() in ~DOMWindow()
}

void DOMWindow::update()
{
	if(timerCallbacks.isEmpty()) return;
	float now = (float)timer.seconds();

	while(TimerCallback* cb = timerCallbacks.findMin())
	{
		if(now < cb->nextInvokeTime())
			break;

		jsval rval;
		if(cb->jsScript)
			JS_ExecuteScript(cb->jsContext, jsObject, cb->jsScript, &rval);
		else
			JS_CallFunctionValue(cb->jsContext, jsObject, cb->closure, 0, NULL, &rval);

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

void DOMWindow::render()
{
	if(!document) return;
	Node* root = document->rootNode();
	if(!root) return;

	// Resize the virtual canvas if needed
	if(virtualCanvas->width() != width())
		virtualCanvas->setWidth(width());
	if(virtualCanvas->height() != height())
		virtualCanvas->setHeight(height());

	for(NodeIterator i(document->rootNode()); !i.ended(); i.next()) {
		i->render();
	}
}

unsigned DOMWindow::width() const
{
	return rhinoca->width;
}

unsigned DOMWindow::height() const
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
	jsObject = JS_NewObject(cx, &jsClass, NULL, NULL);
	VERIFY(JS_SetPrivate(cx, jsObject, this));
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

}	// namespace Dom
