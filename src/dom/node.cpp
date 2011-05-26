#include "pch.h"
#include "node.h"
#include "nodelist.h"
#include "document.h"

namespace Dom {

static void traceDataOp(JSTracer* trc, JSObject* obj)
{
	Node* node = reinterpret_cast<Node*>(JS_GetPrivate(trc->context, obj));
	if(node->firstChild)
		JS_CallTracer(trc, node->firstChild->jsObject, JSTRACE_OBJECT);
	if(node->nextSibling)
		JS_CallTracer(trc, node->nextSibling->jsObject, JSTRACE_OBJECT);
}

JSClass Node::jsClass = {
	"Node", JSCLASS_HAS_PRIVATE | JSCLASS_MARK_IS_TRACE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub, JsBindable::finalize,
	0, 0, 0, 0, 0, 0,
	JS_CLASS_TRACE(traceDataOp),
	0
};

static JSBool appendChild(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	Node* self = reinterpret_cast<Node*>(JS_GetPrivate(cx, obj));

	JSObject* jsChild = NULL;
	VERIFY(JS_ValueToObject(cx, argv[0], &jsChild));
	if(!jsChild) return JS_TRUE;

//	if(!JS_InstanceOf(cx, jsChild, &Node::jsClass, argv)) return JS_TRUE;
	Node* child = reinterpret_cast<Node*>(JS_GetPrivate(cx, jsChild));

	*rval = *self->appendChild(child);
	return JS_TRUE;
}

static JSBool hasChildNodes(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	Node* self = reinterpret_cast<Node*>(JS_GetPrivate(cx, obj));
	*rval = BOOLEAN_TO_JSVAL(self->hasChildNodes());
	return JS_TRUE;
}

static JSBool insertBefore(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	Node* self = reinterpret_cast<Node*>(JS_GetPrivate(cx, obj));

	JSObject* jsNewChild = NULL;
	VERIFY(JS_ValueToObject(cx, argv[0], &jsNewChild));
	if(!jsNewChild) return JS_TRUE;

	if(!JS_InstanceOf(cx, jsNewChild, &Node::jsClass, argv)) return JS_TRUE;
	Node* newChild = reinterpret_cast<Node*>(JS_GetPrivate(cx, jsNewChild));

	JSObject* jsRefChild = NULL;
	VERIFY(JS_ValueToObject(cx, argv[1], &jsRefChild));

	if(!JS_InstanceOf(cx, jsRefChild, &Node::jsClass, argv)) return JS_TRUE;
	Node* refChild = reinterpret_cast<Node*>(JS_GetPrivate(cx, jsRefChild));

	*rval = *self->insertBefore(newChild, refChild);
	return JS_TRUE;
}

static JSBool removeChild(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	Node* self = reinterpret_cast<Node*>(JS_GetPrivate(cx, obj));

	JSObject* jsChild = NULL;
	VERIFY(JS_ValueToObject(cx, argv[0], &jsChild));
	if(!jsChild) return JS_TRUE;

//	if(!JS_InstanceOf(cx, jsChild, &Node::jsClass, argv)) return JS_TRUE;
	Node* child = reinterpret_cast<Node*>(JS_GetPrivate(cx, jsChild));

	*rval = *self->removeChild(child);
	return JS_TRUE;
}

static JSBool replaceChild(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	Node* self = reinterpret_cast<Node*>(JS_GetPrivate(cx, obj));

	JSObject* jsOldChild = NULL;
	VERIFY(JS_ValueToObject(cx, argv[0], &jsOldChild));
	if(!jsOldChild) return JS_TRUE;

//	if(!JS_InstanceOf(cx, jsOldChild, &Node::jsClass, argv)) return JS_TRUE;
	Node* oldChild = reinterpret_cast<Node*>(JS_GetPrivate(cx, jsOldChild));

	JSObject* jsNewChild = NULL;
	VERIFY(JS_ValueToObject(cx, argv[0], &jsNewChild));
	if(!jsNewChild) return JS_TRUE;

//	if(!JS_InstanceOf(cx, jsNewChild, &Node::jsClass, argv)) return JS_TRUE;
	Node* newChild = reinterpret_cast<Node*>(JS_GetPrivate(cx, jsNewChild));

	*rval = *self->replaceChild(oldChild, newChild);
	return JS_TRUE;
}

static JSFunctionSpec methods[] = {
	{"appendChild", appendChild, 1,0,0},
//	{"cloneNode", cloneNode, 0,0,0},
	{"hasChildNodes", hasChildNodes, 0,0,0},	// https://developer.mozilla.org/en/DOM/Node.hasChildNodes
	{"insertBefore", insertBefore, 2,0,0},
	{"removeChild", removeChild, 1,0,0},		// https://developer.mozilla.org/en/DOM/Node.removeChild
	{"replaceChild", replaceChild, 2,0,0},
	{0}
};

static JSBool childNodes(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	Node* self = reinterpret_cast<Node*>(JS_GetPrivate(cx, obj));
	*vp = *self->childNodes();
	return JS_TRUE;
}

static JSBool firstChild(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	Node* self = reinterpret_cast<Node*>(JS_GetPrivate(cx, obj));
	*vp = *self->firstChild;
	return JS_TRUE;
}

static JSBool lastChild(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	Node* self = reinterpret_cast<Node*>(JS_GetPrivate(cx, obj));
	*vp = *self->lastChild();
	return JS_TRUE;
}

static JSBool nextSibling(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	Node* self = reinterpret_cast<Node*>(JS_GetPrivate(cx, obj));
	*vp = *self->nextSibling;
	return JS_TRUE;
}

static JSBool nodeName(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	Node* self = reinterpret_cast<Node*>(JS_GetPrivate(cx, obj));
	*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, self->nodeName.c_str()));
	return JS_TRUE;
}

static JSBool ownerDocument(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	Node* self = reinterpret_cast<Node*>(JS_GetPrivate(cx, obj));
	*vp = *self->ownerDocument;
	return JS_TRUE;
}

static JSBool parentNode(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	Node* self = reinterpret_cast<Node*>(JS_GetPrivate(cx, obj));
	*vp = *self->parentNode;
	return JS_TRUE;
}

static JSBool previousSibling(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	Node* self = reinterpret_cast<Node*>(JS_GetPrivate(cx, obj));
	*vp = *self->previousSibling();
	return JS_TRUE;
}

static JSPropertySpec properties[] = {
	{"childNodes", 0, JSPROP_READONLY, childNodes, JS_PropertyStub},	// NOTE: Current implementation will not return the same NodeList object on each call of childNodes
	{"firstChild", 0, JSPROP_READONLY, firstChild, JS_PropertyStub},
	{"lastChild", 0, JSPROP_READONLY, lastChild, JS_PropertyStub},
	{"nextSibling", 0, JSPROP_READONLY, nextSibling, JS_PropertyStub},
	{"nodeName", 0, JSPROP_READONLY, nodeName, JS_PropertyStub},
	{"ownerDocument", 0, JSPROP_READONLY, ownerDocument, JS_PropertyStub},
	{"parentNode", 0, JSPROP_READONLY, parentNode, JS_PropertyStub},
	{"previousSibling", 0, JSPROP_READONLY, previousSibling, JS_PropertyStub},
	{0}
};

Node::Node()
	: ownerDocument(NULL)
	, parentNode(NULL)
	, firstChild(NULL)
	, nextSibling(NULL)
{
}

Node::~Node()
{
	while(firstChild) firstChild->removeThis();
	if(jsContext) JS_GC(jsContext);
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

	newChild->ownerDocument = ownerDocument;
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

	newChild->ownerDocument = ownerDocument;
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
