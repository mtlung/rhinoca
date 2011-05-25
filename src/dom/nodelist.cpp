#include "pch.h"
#include "nodelist.h"

namespace Dom {

JSClass NodeList::jsClass = {
	"NodeList", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub, JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
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
	{"length", 0, 0, length, JS_PropertyStub},
	{0}
};

NodeList::NodeList(Node* node, Filter filter)
	: root(node)
	, filter(NULL)
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
	if(filter) {
		for(NodeIterator itr(root); !itr.ended(); ) {
			if((*filter)(itr) && (i++) == index)
				return itr.current();
		}
	}
	else {
		for(NodeIterator itr(root); !itr.ended(); itr.next()) {
			if((i++) == index)
				return itr.current();
		}
	}

	return NULL;
}

unsigned NodeList::length()
{
	unsigned count = 0;

	if(filter) {
		for(NodeIterator itr(root); !itr.ended(); ) {
			if((*filter)(itr))
				++count;
		}
	}
	else {
		for(NodeIterator itr(root); !itr.ended(); itr.next()) {
			++count;
		}
	}

	return count;
}

}	// namespace Dom
