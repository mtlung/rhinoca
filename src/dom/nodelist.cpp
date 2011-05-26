#include "pch.h"
#include "nodelist.h"

namespace Dom {

static void traceDataOp(JSTracer* trc, JSObject* obj)
{
	NodeList* nodeList = reinterpret_cast<NodeList*>(JS_GetPrivate(trc->context, obj));
	JS_CallTracer(trc, nodeList->root->jsObject, JSTRACE_OBJECT);
}

static JSBool getProperty(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	NodeList* self = reinterpret_cast<NodeList*>(JS_GetPrivate(cx, obj));

	int32 index;
	if(JS_ValueToInt32(cx, id, &index)) {
		*vp = *self->item(index);
		return JS_TRUE;
	}

	return JS_FALSE;
}

JSClass NodeList::jsClass = {
	"NodeList", JSCLASS_HAS_PRIVATE | JSCLASS_MARK_IS_TRACE,
	JS_PropertyStub, JS_PropertyStub, getProperty, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub, JsBindable::finalize,
	0, 0, 0, 0, 0, 0,
	JS_CLASS_TRACE(traceDataOp),
	0
};

static JSBool item(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	NodeList* self = reinterpret_cast<NodeList*>(JS_GetPrivate(cx, obj));

	int32 index;
	if(!JS_ValueToInt32(cx, argv[0], &index)) return JS_FALSE;

	*rval = *self->item(index);
	return JS_TRUE;
}

static JSFunctionSpec methods[] = {
	{"item", item, 1,0,0},
	{0}
};

static JSBool length(JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	NodeList* self = reinterpret_cast<NodeList*>(JS_GetPrivate(cx, obj));
	*vp = INT_TO_JSVAL(self->length());
	return JS_TRUE;
}

static JSPropertySpec properties[] = {
	{"length", 0, JSPROP_READONLY, length, JS_PropertyStub},
	{0}
};

NodeList::NodeList(Node* node, Filter filter, void* fileterUserData)
	: root(node)
	, filter(filter)
	, userData(fileterUserData)
{
	ASSERT(root);
}

NodeList::~NodeList()
{
}

void NodeList::bind(JSContext* cx, JSObject* parent)
{
	ASSERT(!jsContext);
	jsContext = cx;
	jsObject = JS_NewObject(cx, &jsClass, NULL, parent);
	VERIFY(JS_SetPrivate(cx, *this, this));
	VERIFY(JS_DefineFunctions(cx, *this, methods));
	VERIFY(JS_DefineProperties(cx, *this, properties));
	addReference();	// releaseReference() in JsBindable::finalize()
}

Node* NodeList::item(unsigned index)
{
	unsigned i = 0;
	NodeIterator itr(root);
	itr.next();	// Skip the root itself

	if(filter) {
		for(; !itr.ended(); ) {
			if((*filter)(itr, userData) && (i++) == index)
				return itr.current();
		}
	}
	else {
		for(; !itr.ended(); itr.next()) {
			if((i++) == index)
				return itr.current();
		}
	}

	return NULL;
}

unsigned NodeList::length()
{
	unsigned count = 0;
	NodeIterator itr(root);
	itr.next();	// Skip the root itself

	if(filter) {
		for(; !itr.ended(); ) {
			if((*filter)(itr, userData))
				++count;
		}
	}
	else {
		for(; !itr.ended(); itr.next()) {
			++count;
		}
	}

	return count;
}

}	// namespace Dom
