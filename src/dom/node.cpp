#include "pch.h"
#include "node.h"
#include "document.h"

namespace Dom {

JSClass Node::jsClass = {
	"Node", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub, JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSBool appendChild(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	Node* self = reinterpret_cast<Node*>(JS_GetPrivate(cx, obj));

	JSObject* jsChild = NULL;
	VERIFY(JS_ValueToObject(cx, argv[0], &jsChild));

	if(!JS_InstanceOf(cx, jsChild, &Node::jsClass, argv)) return JS_FALSE;
	Node* child = reinterpret_cast<Node*>(JS_GetPrivate(cx, jsChild));

	self->appendChild(child);

	*rval = OBJECT_TO_JSVAL(child->jsObject);

	return JS_TRUE;
}

static JSBool insertBefore(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	Node* self = reinterpret_cast<Node*>(JS_GetPrivate(cx, obj));

	JSObject* jsNewChild = NULL;
	VERIFY(JS_ValueToObject(cx, argv[0], &jsNewChild));

	if(!JS_InstanceOf(cx, jsNewChild, &Node::jsClass, argv)) return JS_FALSE;
	Node* newChild = reinterpret_cast<Node*>(JS_GetPrivate(cx, jsNewChild));

	JSObject* jsRefChild = NULL;
	VERIFY(JS_ValueToObject(cx, argv[1], &jsRefChild));

	if(!JS_InstanceOf(cx, jsRefChild, &Node::jsClass, argv)) return JS_FALSE;
	Node* refChild = reinterpret_cast<Node*>(JS_GetPrivate(cx, jsRefChild));

	self->insertBefore(newChild, refChild);

	*rval = OBJECT_TO_JSVAL(jsNewChild);

	return JS_TRUE;
}

static JSFunctionSpec methods[] = {
	{"appendChild", appendChild, 1,0,0},
	{"insertBefore", insertBefore, 2,0,0},
	{0}
};

static JSBool nodeName(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	Node* node = reinterpret_cast<Node*>(JS_GetPrivate(cx, obj));
	*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, node->nodeName.c_str()));
	return JS_TRUE;
}

static JSBool ownerDocument(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	Node* node = reinterpret_cast<Node*>(JS_GetPrivate(cx, obj));
	*vp = OBJECT_TO_JSVAL(node->ownerDocument->jsObject);
	return JS_TRUE;
}

static JSPropertySpec properties[] = {
	{"nodeName", 0, 0, nodeName, JS_PropertyStub},
	{"ownerDocument", 0, 0, ownerDocument, JS_PropertyStub},
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
	JS_GC(jsContext);
}

void Node::bind(JSContext* cx, JSObject* parent)
{
	ASSERT(!jsContext);
	jsContext = cx;
	jsObject = JS_NewObject(cx, &jsClass, NULL, parent);
	VERIFY(JS_SetPrivate(cx, jsObject, this));
	VERIFY(JS_DefineFunctions(cx, jsObject, methods));
//	JS_DefineProperties(cx, jsObject, Canvas_properties);
	addReference();
}

JSObject* Node::createPrototype()
{
	ASSERT(jsContext);
	JSObject* proto = JS_NewObject(jsContext, &jsClass, NULL, NULL);
	VERIFY(JS_SetPrivate(jsContext, proto, this));
	VERIFY(JS_DefineFunctions(jsContext, proto, methods));
	addReference();
	return proto;
}

void Node::appendChild(Node* child)
{
	ASSERT(!child->parentNode);

	child->parentNode = this;

	if(Node* n = lastChild())
		n->nextSibling = child;
	else
		firstChild = child;

	child->ownerDocument = ownerDocument;
	child->bind(jsContext, NULL);
	child->addGcRoot();	// releaseGcRoot() in Node::removeThis()
}

void Node::insertBefore(Node* newChild, Node* refChild)
{
	ASSERT(!newChild->parentNode);
	ASSERT(refChild->parentNode == this);

	newChild->parentNode = this;
	newChild->nextSibling = refChild;

	if(Node* n = refChild->previousSibling())
		n->nextSibling = newChild;
	else
		firstChild = newChild;

	newChild->ownerDocument = ownerDocument;
	newChild->bind(jsContext, NULL);
	newChild->addGcRoot();	// releaseGcRoot() in Node::removeThis()
}

void Node::removeThis()
{
//	if(!parentNode) return;

	if(Node* previous = previousSibling())
		previous->nextSibling = nextSibling;

	if(parentNode && parentNode->firstChild == this)
		parentNode->firstChild = nextSibling;

	parentNode = NULL;
	nextSibling = NULL;

	releaseGcRoot();
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
