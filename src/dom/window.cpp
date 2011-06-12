#include "pch.h"
#include "window.h"
#include "body.h"
#include "canvas.h"
#include "document.h"
#include "mouseevent.h"
#include "node.h"
#include "windowlocation.h"
#include "../common.h"
#include "../context.h"
#include "../vector.h"

extern rhinoca_alertFunc alertFunc;
extern void* alertFuncUserData;

namespace Dom {

static void traceDataOp(JSTracer* trc, JSObject* obj)
{
	Window* self = getJsBindable<Window>(trc->context, obj);

	self->EventTarget::jsTrace(trc);

	if(self->document)
		JS_CALL_OBJECT_TRACER(trc, self->document->jsObject, "Window.document");

	for(FrameRequestCallback* cb = self->frameRequestCallbacks.begin(); cb != self->frameRequestCallbacks.end(); cb = cb->next())
		JS_CALL_OBJECT_TRACER(trc, *cb, "Window.frameRequestCallback[i]");

	for(TimerCallback* cb = self->timerCallbacks.findMin(); cb != NULL; cb = cb->next())
		JS_CALL_OBJECT_TRACER(trc, *cb, "Window.timerCallbacks[i]");
}

JSClass Window::jsClass = {
	"Window", JSCLASS_HAS_PRIVATE | JSCLASS_MARK_IS_TRACE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub, JsBindable::finalize,
	0, 0, 0, 0, 0, 0,
	JS_CLASS_TRACE(traceDataOp),
	0
};

static JSBool alert(JSContext* cx, uintN argc, jsval* vp)
{
	JsString jss(cx, vp, 0);
	if(alertFunc && jss) {
		Window* self = getJsBindable<Window>(cx, vp);
		alertFunc(self->rhinoca, alertFuncUserData, jss.c_str());
		return JS_TRUE;
	}
	return JS_FALSE;
}

static JSBool compileScript(TimerCallback* cb, JSContext* cx, jsval* vp)
{
	JsString jss(cx, vp, 0);
	cb->jsScript = JS_CompileScript(cx, JS_THIS_OBJECT(cx, vp), jss.c_str(), jss.size(), NULL, 0);
	if(!cb->jsScript) return JS_FALSE;
	VERIFY(JS_SetReservedSlot(cx, *cb, 0, OBJECT_TO_JSVAL(cb->jsScript)));

	return JS_TRUE;
}

JSBool setTimeout(JSContext* cx, uintN argc, jsval* vp)
{
	int32 timeout;	// In unit of millisecond
	if(!JS_ValueToInt32(cx, JS_ARGV1, &timeout)) return JS_FALSE;
	Window* self = getJsBindable<Window>(cx, vp);

	TimerCallback* cb = new TimerCallback;
	cb->bind(cx, NULL);

	if(JSVAL_IS_STRING(JS_ARGV0)) {
		if(!compileScript(cb, cx, vp)) return JS_FALSE;
	}
	else {
		cb->closure = JS_ARGV0;
		VERIFY(JS_SetReservedSlot(cx, *cb, 0, cb->closure));
	}

	cb->interval = 0;
	cb->setNextInvokeTime(float(self->timer.seconds()) + (float)timeout/1000);

	self->timerCallbacks.insert(*cb);

	JS_RVAL(cx, vp) = *cb;

	return JS_TRUE;
}

JSBool setInterval(JSContext* cx, uintN argc, jsval* vp)
{
	int32 interval;	// In unit of millisecond
	if(!JS_ValueToInt32(cx, JS_ARGV1, &interval)) return JS_FALSE;
	Window* self = getJsBindable<Window>(cx, vp);

	TimerCallback* cb = new TimerCallback;
	cb->bind(cx, NULL);

	if(JSVAL_IS_STRING(JS_ARGV0)) {
		if(!compileScript(cb, cx, vp)) return JS_FALSE;
	}
	else {
		cb->closure = JS_ARGV0;
		VERIFY(JS_SetReservedSlot(cx, *cb, 0, cb->closure));
	}

	cb->interval = (float)interval/1000;
	cb->setNextInvokeTime(float(self->timer.seconds()) + cb->interval);

	self->timerCallbacks.insert(*cb);

	JS_RVAL(cx, vp) = *cb;

	return JS_TRUE;
}

JSBool clearTimeout(JSContext* cx, uintN argc, jsval* vp)
{
	TimerCallback* timerCallback = getJsBindable<TimerCallback>(cx, vp, 0);

	if(timerCallback->isInMap())
		timerCallback->removeThis();

	return JS_TRUE;
}

JSBool clearInterval(JSContext* cx, uintN argc, jsval* vp)
{
	TimerCallback* cb = getJsBindable<TimerCallback>(cx, vp, 0);
	if(!cb) return JS_FALSE;

	if(cb->isInMap())
		cb->removeThis();

	return JS_TRUE;
}

// NOTE: For simplicity, we ignore the second parameter to requestAnimationFrame
JSBool requestAnimationFrame(JSContext* cx, uintN argc, jsval* vp)
{
	Window* self = getJsBindable<Window>(cx, vp);

	FrameRequestCallback* cb = new FrameRequestCallback;
	cb->bind(cx, NULL);

	if(!JSVAL_IS_OBJECT(JS_ARGV0))
		return JS_FALSE;

	cb->closure = JS_ARGV0;
	VERIFY(JS_SetReservedSlot(cx, *cb, 0, cb->closure));

	self->frameRequestCallbacks.pushFront(*cb);

	JS_RVAL(cx, vp) = *cb;

	return JS_TRUE;
}

JSBool cancelRequestAnimationFrame(JSContext* cx, uintN argc, jsval* vp)
{
	FrameRequestCallback* cb = getJsBindable<FrameRequestCallback>(cx, vp, 0);
	if(!cb) return JS_FALSE;

	if(cb->isInList())
		cb->removeThis();

	return JS_TRUE;
}

static JSBool addEventListener(JSContext* cx, uintN argc, jsval* vp)
{
	Window* self = getJsBindable<Window>(cx, vp);
	return self->addEventListener(cx, JS_ARGV0, JS_ARGV1, JS_ARGV2);
}

static JSBool removeEventListener(JSContext* cx, uintN argc, jsval* vp)
{
	Window* self = getJsBindable<Window>(cx, vp);
	return self->removeEventListener(cx, JS_ARGV0, JS_ARGV1, JS_ARGV2);
}

static JSBool dispatchEvent(JSContext* cx, uintN argc, jsval* vp)
{
	Window* self = getJsBindable<Window>(cx, vp);

	JSObject* _obj = NULL;
	if(JS_ValueToObject(cx, JS_ARGV0, &_obj) != JS_TRUE) return JS_FALSE;
	Event* ev = reinterpret_cast<Event*>(JS_GetPrivate(cx, _obj));

	self->dispatchEvent(ev);

	return JS_TRUE;
}

static JSFunctionSpec methods[] = {
	{"alert", alert, 1,0},
	{"setTimeout", setTimeout, 2,0},
	{"setInterval", setInterval, 2,0},
	{"clearTimeout", clearTimeout, 1,0},
	{"clearInterval", clearInterval, 1,0},
	{"requestAnimationFrame", requestAnimationFrame, 2,0},
	{"cancelRequestAnimationFrame", cancelRequestAnimationFrame, 1,0},
	{"addEventListener", addEventListener, 3,0},
	{"removeEventListener", removeEventListener, 3,0},
	{"dispatchEvent", dispatchEvent, 1,0},
	{0}
};

static JSBool location(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	Window* self = getJsBindable<Window>(cx, obj);
	WindowLocation* wLoc = new WindowLocation(self);
	wLoc->bind(cx, NULL);
	*vp = *wLoc;
	return JS_TRUE;
}

static const char* _eventAttributeTable[] = {
	"onload",
};

static JSBool getEventAttribute(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	// Not implemented
	return JS_FALSE;
}

static JSBool setEventAttribute(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	Window* self = getJsBindable<Window>(cx, obj);
	int32 idx = JSID_TO_INT(id) / 2 + 0;	// Account for having both get and set functions

	return self->addEventListenerAsAttribute(cx, _eventAttributeTable[idx], *vp);
}

static JSPropertySpec properties[] = {
	{"location", 0, JSPROP_READONLY, location, JS_StrictPropertyStub},

	// Event attributes
	{_eventAttributeTable[0], 0, 0, getEventAttribute, setEventAttribute},
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
}

void Window::dispatchEvent(Event* e)
{
	ASSERT(e->jsObject);

	// Get a list of traversed node first
	Vector<Node*> nodes;
	for(Dom::NodeIterator itr(document); !itr.ended(); itr.next())
		if(itr->hasListener())
			nodes.push_back(itr.current());

	// Handling of mouse events
	// TODO: Generate 'secondary' events like mouse clicked (down and up in the same position)
	if(MouseEvent* mouse = dynamic_cast<MouseEvent*>(e))
	{
		// Loop from the back of nodes to see where the mouse fall into the element's rectangle
		// TODO: Handling of stack context and z-index
		for(unsigned i=nodes.size(); i--; )
		{
			if(Element* ele = dynamic_cast<Element*>(nodes[i]))
			{
				// TODO: Temp solution, before we assign the body element with correct width and height
				if(ele->tagName() == FixString("BODY")) {
					e->target = ele;
					ele->dispatchEvent(e);
					return;
				}

				if(ele->clientLeft() <= mouse->clientX && mouse->clientX <= ele->clientRight())
				if(ele->clientTop() <= mouse->clientY && mouse->clientY <= ele->clientBottom()) {
					e->target = ele;
					ele->dispatchEvent(e);
					return;
				}
			}
		}
	}

	if(e->target)
		e->target->dispatchEvent(e);
}

void Window::update()
{
	rhuint64 usSince1970 = Timer::microSecondsSince1970();

	// Invoke animation request callbacks
	for(FrameRequestCallback* cb = frameRequestCallbacks.begin(); cb != frameRequestCallbacks.end();)
	{
		jsval rval;
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
	"TimerCallback", JSCLASS_HAS_PRIVATE | JSCLASS_HAS_RESERVED_SLOTS(1),
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub, JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
};

TimerCallback::TimerCallback()
	: closure(JSVAL_VOID), jsScript(NULL)
{
}

TimerCallback::~TimerCallback()
{
	ASSERT(closure == JSVAL_VOID);
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
	closure = JSVAL_VOID;
	jsScript = NULL;
}

JSClass FrameRequestCallback::jsClass = {
	"FrameRequestCallback", JSCLASS_HAS_PRIVATE | JSCLASS_HAS_RESERVED_SLOTS(1),
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub, JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
};

FrameRequestCallback::FrameRequestCallback()
	: closure(JSVAL_VOID)
{
}

FrameRequestCallback::~FrameRequestCallback()
{
	ASSERT(closure == JSVAL_VOID);
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
	closure = JSVAL_VOID;
}

}	// namespace Dom
