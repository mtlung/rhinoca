#include "pch.h"
#include "window.h"
#include "body.h"
#include "canvas.h"
#include "canvas2dcontext.h"
#include "document.h"
#include "keyevent.h"
#include "mouseevent.h"
#include "touchevent.h"
#include "navigator.h"
#include "node.h"
#include "windowlocation.h"
#include "windowscreen.h"
#include "../common.h"
#include "../context.h"
#include "../vector.h"

extern rhinoca_alertFunc alertFunc;
extern void* alertFuncUserData;

namespace Dom {

static void traceDataOp(JSTracer* trc, JSObject* obj)
{
	Window* self = getJsBindable<Window>(trc->context, obj);

	// NOTE: Got some invocation with self == null, after introducing  Window::registerClass()
	if(!self) return;

	self->EventTarget::jsTrace(trc);

	if(self->document)
		JS_CALL_OBJECT_TRACER(trc, self->document->jsObject, "Window.document");

	for(unsigned i=0; i<self->touches.size(); ++i) {
		if(EventTarget* t = self->touches[i].target)
			JS_CALL_OBJECT_TRACER(trc, t->getJSObject(), "Window.frameRequestCallback[i]");
	}

	for(FrameRequestCallback* cb = self->frameRequestCallbacks.begin(); cb != self->frameRequestCallbacks.end(); cb = cb->next())
		JS_CALL_OBJECT_TRACER(trc, *cb, "Window.frameRequestCallback[i]");

	for(TimerCallback* cb = self->timerCallbacks.findMin(); cb != NULL; cb = cb->next())
		JS_CALL_OBJECT_TRACER(trc, *cb, "Window.timerCallbacks[i]");
}

JSClass Window::jsClass = {
	"Window", JSCLASS_GLOBAL_FLAGS | JSCLASS_HAS_PRIVATE | JSCLASS_MARK_IS_TRACE,
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
	RHVERIFY(JS_SetReservedSlot(cx, *cb, 0, OBJECT_TO_JSVAL(cb->jsScript)));

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
		RHVERIFY(JS_SetReservedSlot(cx, *cb, 0, cb->closure));
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
		RHVERIFY(JS_SetReservedSlot(cx, *cb, 0, cb->closure));
	}

	cb->interval = (float)interval/1000;
	cb->setNextInvokeTime(float(self->timer.seconds()) + cb->interval);

	self->timerCallbacks.insert(*cb);

	JS_RVAL(cx, vp) = *cb;

	return JS_TRUE;
}

JSBool clearTimeout(JSContext* cx, uintN argc, jsval* vp)
{
	// Just skip silently if the argument isn't a function
	if(!JSVAL_IS_OBJECT(JS_ARGV0))
		return JS_TRUE;

	TimerCallback* timerCallback = getJsBindable<TimerCallback>(cx, vp, 0);

	if(timerCallback->isInMap())
		timerCallback->removeThis();

	return JS_TRUE;
}

JSBool clearInterval(JSContext* cx, uintN argc, jsval* vp)
{
	// Just skip silently if the argument isn't a function
	if(!JSVAL_IS_OBJECT(JS_ARGV0))
		return JS_TRUE;

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
	RHVERIFY(JS_SetReservedSlot(cx, *cb, 0, cb->closure));

	self->frameRequestCallbacks.pushFront(*cb);

	JS_RVAL(cx, vp) = *cb;

	return JS_TRUE;
}

JSBool cancelRequestAnimationFrame(JSContext* cx, uintN argc, jsval* vp)
{
	// Just skip silently if the argument isn't a function
	if(!JSVAL_IS_OBJECT(JS_ARGV0))
		return JS_TRUE;

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

static JSBool document(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	Window* self = getJsBindable<Window>(cx, obj);
	*vp = *self->document;
	return JS_TRUE;
}

static JSBool event(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	Window* self = getJsBindable<Window>(cx, obj);
	*vp = *self->currentEvent;
	return JS_TRUE;
}

static JSBool location(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	Window* self = getJsBindable<Window>(cx, obj);
	WindowLocation* wLoc = new WindowLocation(self);
	wLoc->bind(cx, NULL);
	*vp = *wLoc;
	return JS_TRUE;
}

static JSBool navigator(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	Window* self = getJsBindable<Window>(cx, obj);
	Navigator* navigator = new Navigator;
	navigator->userAgent = self->rhinoca->userAgent;
	navigator->bind(cx, NULL);
	*vp = *navigator;
	return JS_TRUE;
}

static JSBool screen(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	Window* self = getJsBindable<Window>(cx, obj);
	WindowScreen* screen = new WindowScreen(self);
	screen->bind(cx, NULL);
	*vp = *screen;
	return JS_TRUE;
}

static JSBool innerWidth(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	Window* self = getJsBindable<Window>(cx, obj);
	*vp = INT_TO_JSVAL(self->width()); return JS_TRUE;
}

static JSBool innerHeight(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	Window* self = getJsBindable<Window>(cx, obj);
	*vp = INT_TO_JSVAL(self->height()); return JS_TRUE;
}

static const char* _eventAttributeTable[] = {
	"onload",
	"onmouseup",
	"onmousedown",
	"onmousemove",
	"onkeydown",
	"onkeyup"
};

static JSBool getEventAttribute(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	Window* self = getJsBindable<Window>(cx, obj);
	int32 idx = JSID_TO_INT(id);

	*vp = self->getEventListenerAsAttribute(cx, _eventAttributeTable[idx]);
	return JS_TRUE;
}

static JSBool setEventAttribute(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	Window* self = getJsBindable<Window>(cx, obj);
	int32 idx = JSID_TO_INT(id);

	return self->addEventListenerAsAttribute(cx, _eventAttributeTable[idx], *vp);
}

static JSPropertySpec properties[] = {
	{"document", 0, JSPROP_READONLY | JsBindable::jsPropFlags, document, JS_StrictPropertyStub},
	{"event", 0, JSPROP_READONLY | JsBindable::jsPropFlags, event, JS_StrictPropertyStub},
	{"location", 0, JSPROP_READONLY | JsBindable::jsPropFlags, location, JS_StrictPropertyStub},
	{"navigator", 0, JSPROP_READONLY | JsBindable::jsPropFlags, navigator, JS_StrictPropertyStub},
	{"screen", 0, JSPROP_READONLY | JsBindable::jsPropFlags, screen, JS_StrictPropertyStub},
	{"innerWidth", 0, JSPROP_READONLY | JsBindable::jsPropFlags, innerWidth, JS_StrictPropertyStub},
	{"innerHeight", 0, JSPROP_READONLY | JsBindable::jsPropFlags, innerHeight, JS_StrictPropertyStub},

	// Event attributes
	{_eventAttributeTable[0], 0, JsBindable::jsPropFlags, getEventAttribute, setEventAttribute},
	{_eventAttributeTable[1], 1, JsBindable::jsPropFlags, getEventAttribute, setEventAttribute},
	{_eventAttributeTable[2], 2, JsBindable::jsPropFlags, getEventAttribute, setEventAttribute},
	{_eventAttributeTable[3], 3, JsBindable::jsPropFlags, getEventAttribute, setEventAttribute},
	{_eventAttributeTable[4], 4, JsBindable::jsPropFlags, getEventAttribute, setEventAttribute},
	{_eventAttributeTable[5], 5, JsBindable::jsPropFlags, getEventAttribute, setEventAttribute},
	{0}
};

Window::Window(Rhinoca* rh)
	: rhinoca(rh)
	, currentEvent(NULL)
{
	document = new HTMLDocument(rh);
	virtualCanvas = new HTMLCanvasElement(rh);
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
	RHASSERT(!jsContext);
	RHASSERT(jsObject);

	jsContext = cx;
	RHVERIFY(JS_SetPrivate(cx, *this, this));
	RHVERIFY(JS_DefineFunctions(cx, *this, methods));
	RHVERIFY(JS_DefineProperties(cx, *this, properties));
	addReference();

	document->bind(cx, parent);
}

static bool isInsideElement(Element* ele, int x, int y)
{
	if(!ele)
		return false;

	// TODO: Temp solution, before we assign the body element with correct width and height
	bool isBodyTag = ele->tagName() == FixString("BODY");
	bool isInBound = (ele->left() <= x && x <= ele->right()) && (ele->top() <= y && y <= ele->bottom());

	return isBodyTag || isInBound;
}

void Window::dispatchEvent(Event* e)
{
	RHASSERT(e->jsObject);

	// Get a list of traversed node first
	PreAllocVector<EventTarget*, 64> targets;

	targets.push_back(this);
	for(Dom::NodeIterator itr(document); !itr.ended(); itr.next())
		if(itr->hasListener())
			targets.push_back(itr.current());

	// Handling of mouse events
	// TODO: Generate 'secondary' events like mouse clicked (down and up in the same position)
	if(MouseEvent* mouse = dynamic_cast<MouseEvent*>(e))
	{
		// Loop from the back of targets to see where the mouse fall into the element's rectangle
		// TODO: Handling of stack context and z-index
		for(unsigned i=targets.size(); i--; )
		{
			Element* ele = dynamic_cast<Element*>(targets[i]);
			if(isInsideElement(ele, mouse->clientX, mouse->clientY)) {
				e->target = ele;
				ele->dispatchEvent(e);
				return;
			}
		}
	}

	// Handling of touchevents
	// TODO: Generate 'secondary' events like mouse clicked (down and up in the same position)
	if(TouchEvent* touch = dynamic_cast<TouchEvent*>(e))
	{
		// Determine which Element the touchBegin occur
		// Loop from the back of targets to see where the mouse fall into the element's rectangle
		// TODO: Handling of stack context and z-index
		if(touch->type == StringHash("touchstart"))
		for(unsigned i=0; i<touch->changedTouches.size(); ++i)
		{
			const TouchData& touchData = touch->changedTouches[i];
			if(touchData.target)
				continue;

			for(unsigned j=targets.size(); j--; )
			{
				if(Element* ele = dynamic_cast<Element*>(targets[j])) {
					if(isInsideElement(ele, touchData.clientX, touchData.clientY)) {
						touches[touchData.index].target = ele;
						break;
					}
				}
			}
		}

		// For each changedTouches, perform dispatch using TouchData::target as the target.
		for(unsigned i=0; i<touch->changedTouches.size(); ++i)
		{
			const TouchData& touchData = touches[touch->changedTouches[i].index];

			if(!touchData.target)
				continue;

			// Construct the targetTouches list
			touch->targetTouches.clear();
			for(unsigned k=0; k<touches.size(); ++k)
				if(touches[k].target == touchData.target)
					touch->targetTouches.push_back(touches[k]);

			e->target = touchData.target;
			e->target->dispatchEvent(e);
		}

		// Do cleanup for "touchend" and "touchcancel"
		if(touch->type == StringHash("touchend") || touch->type == StringHash("touchcancel")) {
			for(unsigned i=0; i<touch->changedTouches.size(); ++i)
			{
				const TouchData& touchData = touch->changedTouches[i];
				touches[touchData.index].identifier = -1;
				touches[touchData.index].target = NULL;
			}
		}
	}

	if(KeyEvent* key = dynamic_cast<KeyEvent*>(e))
	{
		// TODO: Handling of keyboard focus
		for(unsigned i=targets.size(); i--; )
		{
			if(Element* ele = dynamic_cast<Element*>(targets[i]))
			{
				// TODO: Temp solution, before we have the concept of input focus
				if(ele->tagName() == FixString("BODY")) {
					e->target = ele;
					ele->dispatchEvent(e);
					return;
				}
			}
			else if(HTMLDocument* doc = dynamic_cast<HTMLDocument*>(targets[i]))
			{
				e->target = doc;
				doc->dispatchEvent(e);
				return;
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
		jsval argv = DOUBLE_TO_JSVAL(double(usSince1970 / 1000));
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

	// Clear the virtual canvas first
	CanvasRenderingContext2D* ctx = dynamic_cast<CanvasRenderingContext2D*>(virtualCanvas->context);
	if(virtualCanvas->clearEveryFrame)
		ctx->clearRect(0, 0, (float)width(), (float)height());

	// If no other canvas which use  "frontBufferOnly" clearEveryFrame will still be true on next frame
	virtualCanvas->clearEveryFrame = true;

	for(NodeIterator i(document); !i.ended(); i.next()) {
		i->render(ctx);
	}
}

TouchData& Window::allocateTouchData(int identifier)
{
	int index = findTouchIndexByIdentifier(identifier);

	if(index >= 0 && index < (int)touches.size())
		return touches[index];

	// Reuse free slot
	for(unsigned i=0; i<touches.size(); ++i) {
		if(touches[i].identifier < 0) {
			touches[i].identifier = identifier;
			return touches[i];
		}
	}

	// Allocate new entry
	touches.resize(touches.size() + 1);
	touches.back().index = touches.size() - 1;
	touches.back().identifier = identifier;

	return touches.back();
}

int Window::findTouchIndexByIdentifier(int identifier) const
{
	for(unsigned i=0; i<touches.size(); ++i) {
		RHASSERT(touches[i].index == i);
		if(touches[i].identifier == identifier)
			return i;
	}

	return -1;
}

static JSBool construct(JSContext* cx, uintN argc, jsval* vp)
{
	RHASSERT(false && "For compatible with javascript instanceof operator only, you are not suppose to new a Window directly");
	return JS_FALSE;
}

void Window::registerClass(JSContext* cx, JSObject* parent)
{
	RHVERIFY(JS_InitClass(cx, parent, NULL, &jsClass, construct, 0, NULL, NULL, NULL, NULL));
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
	RHASSERT(closure == JSVAL_VOID);
	RHASSERT(jsScript == NULL);
}

void TimerCallback::bind(JSContext* cx, JSObject* parent)
{
	RHASSERT(!jsContext);
	jsContext = cx;
	jsObject = JS_NewObject(cx, &jsClass, NULL, parent);
	RHVERIFY(JS_SetPrivate(cx, *this, this));
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
	RHASSERT(closure == JSVAL_VOID);
}

void FrameRequestCallback::bind(JSContext* cx, JSObject* parent)
{
	RHASSERT(!jsContext);
	jsContext = cx;
	jsObject = JS_NewObject(cx, &jsClass, NULL, parent);
	RHVERIFY(JS_SetPrivate(cx, *this, this));
	addReference();
}

void FrameRequestCallback::removeThis()
{
	super::removeThis();
	closure = JSVAL_VOID;
}

}	// namespace Dom
