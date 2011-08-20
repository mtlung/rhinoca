#include "pch.h"
#include "node.h"
#include "nodelist.h"
#include "document.h"
#include "../context.h"

namespace Dom {

static void traceDataOp(JSTracer* trc, JSObject* obj)
{
	Node* self = getJsBindable<Node>(trc->context, obj);

	self->EventTarget::jsTrace(trc);

	if(self->firstChild)
		JS_CALL_OBJECT_TRACER(trc, self->firstChild->jsObject, "Node.firstChild");
	if(self->nextSibling)
		JS_CALL_OBJECT_TRACER(trc, self->nextSibling->jsObject, "Node.nextSibling");
}

JSClass Node::jsClass = {
	"Node", JSCLASS_HAS_PRIVATE | JSCLASS_MARK_IS_TRACE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub, JsBindable::finalize,
	0, 0, 0, 0, 0, 0,
	JS_CLASS_TRACE(traceDataOp),
	0
};

static JSBool appendChild(JSContext* cx, uintN argc, jsval* vp)
{
	Node* self = getJsBindable<Node>(cx, vp);
	Node* child = getJsBindable<Node>(cx, vp, 0);

	JS_RVAL(cx, vp) = *self->appendChild(child);
	return JS_TRUE;
}

static JSBool hasChildNodes(JSContext* cx, uintN argc, jsval* vp)
{
	Node* self = getJsBindable<Node>(cx, vp);
	JS_RVAL(cx, vp) = BOOLEAN_TO_JSVAL(self->hasChildNodes());
	return JS_TRUE;
}

static JSBool insertBefore(JSContext* cx, uintN argc, jsval* vp)
{
	Node* self = getJsBindable<Node>(cx, vp);
	Node* newChild = getJsBindable<Node>(cx, vp, 0);
	Node* refChild = getJsBindable<Node>(cx, vp, 1);

	JS_RVAL(cx, vp) = *self->insertBefore(newChild, refChild);
	return JS_TRUE;
}

static JSBool removeChild(JSContext* cx, uintN argc, jsval* vp)
{
	Node* self = getJsBindable<Node>(cx, vp);
	Node* child = getJsBindable<Node>(cx, vp, 0);

	JS_RVAL(cx, vp) = *self->removeChild(child);
	return JS_TRUE;
}

static JSBool replaceChild(JSContext* cx, uintN argc, jsval* vp)
{
	Node* self = getJsBindable<Node>(cx, vp);
	Node* oldChild = getJsBindable<Node>(cx, vp, 0);
	Node* newChild = getJsBindable<Node>(cx, vp, 1);

	JS_RVAL(cx, vp) = *self->replaceChild(oldChild, newChild);
	return JS_TRUE;
}

static JSBool cloneNode(JSContext* cx, uintN argc, jsval* vp)
{
	bool recursive= true;
//	if(!JS_GetValue(cx, JS_ARGV0, recursive)) return JS_FALSE;

	Node* self = getJsBindable<Node>(cx, vp);
	Node* newNode = self->cloneNode(recursive);
	ASSERT(!newNode || newNode->jsContext && "Overrided cloneNode() is responsible to bind the object");

	JS_RVAL(cx, vp) = *newNode;
	return JS_TRUE;
}

static JSBool addEventListener(JSContext* cx, uintN argc, jsval* vp)
{
	Node* self = getJsBindable<Node>(cx, vp);
	return self->addEventListener(cx, JS_ARGV0, JS_ARGV1, JS_ARGV2);
}

static JSBool removeEventListener(JSContext* cx, uintN argc, jsval* vp)
{
	Node* self = getJsBindable<Node>(cx, vp);
	return self->removeEventListener(cx, JS_ARGV0, JS_ARGV1, JS_ARGV2);
}

static JSBool dispatchEvent(JSContext* cx, uintN argc, jsval* vp)
{
	Node* self = getJsBindable<Node>(cx, vp);

	JSObject* _obj = NULL;
	if(JS_ValueToObject(cx, JS_ARGV0, &_obj) != JS_TRUE) return JS_FALSE;
	Event* ev = reinterpret_cast<Event*>(JS_GetPrivate(cx, _obj));

	return self->dispatchEvent(ev);
}

static JSFunctionSpec methods[] = {
	{"appendChild", appendChild, 1,0},
//	{"cloneNode", cloneNode, 0,0},
	{"hasChildNodes", hasChildNodes, 0,0},	// https://developer.mozilla.org/en/DOM/Node.hasChildNodes
	{"insertBefore", insertBefore, 2,0},
	{"removeChild", removeChild, 1,0},		// https://developer.mozilla.org/en/DOM/Node.removeChild
	{"replaceChild", replaceChild, 2,0},
	{"cloneNode", cloneNode, 1,0},
	{"addEventListener", addEventListener, 3,0},
	{"removeEventListener", removeEventListener, 3,0},
	{"dispatchEvent", dispatchEvent, 1,0},
	{0}
};

static JSBool childNodes(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	Node* self = getJsBindable<Node>(cx, obj);
	*vp = *self->childNodes();
	return JS_TRUE;
}

static JSBool firstChild(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	Node* self = getJsBindable<Node>(cx, obj);
	*vp = *self->firstChild;
	return JS_TRUE;
}

static JSBool lastChild(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	Node* self = getJsBindable<Node>(cx, obj);
	*vp = *self->lastChild();
	return JS_TRUE;
}

static JSBool nextSibling(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	Node* self = getJsBindable<Node>(cx, obj);
	*vp = *self->nextSibling;
	return JS_TRUE;
}

static JSBool nodeName(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	Node* self = getJsBindable<Node>(cx, obj);
	*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, self->nodeName));
	return JS_TRUE;
}

static JSBool ownerDocument(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	Node* self = getJsBindable<Node>(cx, obj);
	*vp = *self->ownerDocument();
	return JS_TRUE;
}

static JSBool parentNode(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	Node* self = getJsBindable<Node>(cx, obj);
	*vp = *self->parentNode;
	return JS_TRUE;
}

static JSBool previousSibling(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	Node* self = getJsBindable<Node>(cx, obj);
	*vp = *self->previousSibling();
	return JS_TRUE;
}

static JSPropertySpec properties[] = {
	{"childNodes", 0, JSPROP_READONLY | JsBindable::jsPropFlags, childNodes, JS_StrictPropertyStub},	// NOTE: Current implementation will not return the same NodeList object on each call of childNodes
	{"firstChild", 0, JSPROP_READONLY | JsBindable::jsPropFlags, firstChild, JS_StrictPropertyStub},
	{"lastChild", 0, JSPROP_READONLY | JsBindable::jsPropFlags, lastChild, JS_StrictPropertyStub},
	{"nextSibling", 0, JSPROP_READONLY | JsBindable::jsPropFlags, nextSibling, JS_StrictPropertyStub},
	{"nodeName", 0, JSPROP_READONLY | JsBindable::jsPropFlags, nodeName, JS_StrictPropertyStub},
	{"ownerDocument", 0, JSPROP_READONLY | JsBindable::jsPropFlags, ownerDocument, JS_StrictPropertyStub},
	{"parentNode", 0, JSPROP_READONLY | JsBindable::jsPropFlags, parentNode, JS_StrictPropertyStub},
	{"previousSibling", 0, JSPROP_READONLY | JsBindable::jsPropFlags, previousSibling, JS_StrictPropertyStub},
	{0}
};

Node::Node(Rhinoca* rh)
	: rhinoca(rh)
	, parentNode(NULL)
	, firstChild(NULL)
	, nextSibling(NULL)
{
}

Node::~Node()
{
	while(firstChild) firstChild->removeThis();
//	if(jsContext) JS_GC(jsContext);
}

void Node::bind(JSContext* cx, JSObject* parent)
{
	ASSERT(!jsContext);
	jsContext = cx;
	jsObject = JS_NewObject(cx, &jsClass, NULL, parent);
	VERIFY(JS_SetPrivate(cx, *this, this));
	VERIFY(JS_DefineFunctions(cx, *this, methods));
	VERIFY(JS_DefineProperties(cx, *this, properties));
	addReference();	// releaseReference() in JsBindable::finalize()
}

JSObject* Node::createPrototype()
{
	ASSERT(jsContext);
	JSObject* proto = JS_NewObject(jsContext, &jsClass, NULL, NULL);
	VERIFY(JS_SetPrivate(jsContext, proto, this));
	VERIFY(JS_DefineFunctions(jsContext, proto, methods));
	VERIFY(JS_DefineProperties(jsContext, proto, properties));
	addReference();	// releaseReference() in JsBindable::finalize()
	return proto;
}

Node* Node::appendChild(Node* newChild)
{
	// 'newChild' may already bind to JS
	if(!newChild->jsContext)
		newChild->bind(jsContext, NULL);
	newChild->addReference();	// releaseReference() in Node::removeThis()

	newChild->removeThis();
	newChild->parentNode = this;

	if(Node* n = lastChild())
		n->nextSibling = newChild;
	else
		firstChild = newChild;

	return newChild;
}

Node* Node::insertBefore(Node* newChild, Node* refChild)
{
	if(!newChild) return NULL;
	if(refChild && refChild->parentNode != this) return NULL;

	if(!newChild->jsContext)
		newChild->bind(jsContext, NULL);
	newChild->addReference();	// releaseReference() in Node::removeThis()

	newChild->removeThis();
	newChild->parentNode = this;
	newChild->nextSibling = refChild;

	if(!refChild)
		lastChild()->nextSibling = newChild;
	else if(Node* n = refChild->previousSibling())
		n->nextSibling = newChild;
	else
		firstChild = newChild;

	return newChild;
}

Node* Node::replaceChild(Node* oldChild, Node* newChild)
{
	insertBefore(newChild, oldChild->nextSibling);
	oldChild->removeThis();
	return oldChild;
}

Node* Node::removeChild(Node* child)
{
	if(!child || child->parentNode != this) return NULL;
	child->removeThis();
	return child;
}

void Node::removeThis()
{
	if(!parentNode) return;

	if(Node* previous = previousSibling())
		previous->nextSibling = nextSibling;

	if(parentNode && parentNode->firstChild == this)
		parentNode->firstChild = nextSibling;

	parentNode = NULL;
	nextSibling = NULL;

	releaseReference();
}

Node* Node::cloneNode(bool recursive)
{
	return NULL;
}

EventTarget* Node::eventTargetTraverseUp()
{
	return parentNode;
}

void Node::eventTargetAddReference() {
	addReference();
}

void Node::eventTargetReleaseReference() {
	releaseReference();
}

HTMLDocument* Node::ownerDocument() const {
	return rhinoca->domWindow->document;
}

static Node* childNodesFilter(NodeIterator& iter, void* userData)
{
	Node* ret = iter.current();
	iter.skipChildren();
	return ret;
}

NodeList* Node::childNodes()
{
	NodeList* list = new NodeList(this, childNodesFilter);
	list->bind(jsContext, NULL);
	return list;
}

Node* Node::lastChild()
{
	Node* n = firstChild;
	while(n) {
		if(!n->nextSibling)
			return n;
		n = n->nextSibling;
	}
	return NULL;
}

Node* Node::previousSibling()
{
	if(!parentNode) return NULL;
	Node* n = parentNode->firstChild;
	while(n) {
		if(n->nextSibling == this)
			return n;
		n = n->nextSibling;
	}
	return NULL;
}

bool Node::hasChildNodes() const
{
	return firstChild != NULL;
}

NodeIterator::NodeIterator(Node* start)
	: _current(start), _start(start)
{}

NodeIterator::NodeIterator(Node* start, Node* current)
	: _current(current), _start(start)
{}

Node* NodeIterator::next()
{
	// After an upward movement is preformed, we will not visit the child again
	bool noChildMove = false;

	while(_current)
	{
		if(_current->firstChild && !noChildMove)
			return _current = _current->firstChild;
		else if(_current == _start)
			_current = NULL;
		else if(_current->nextSibling)
			return _current = _current->nextSibling;
		else
		{
			_current = _current->parentNode;
			noChildMove = true;

			if(_current == _start || (_start && _current == _start->parentNode))
				_current = NULL;
		}
	}

	return _current;
}

Node* NodeIterator::skipChildren()
{
	// The following borrows the code from next() function, where noChildMove is always true
	while(_current)
	{
		if(_current->nextSibling)
			return _current = _current->nextSibling;

		_current = _current->parentNode;
		if(_current == _start)
			_current = NULL;
	}

	return _current;
}

}	// namespace Dom
