#include "pch.h"
#include "textnode.h"

namespace Dom {

JSClass TextNode::jsClass = {
	"TextNode", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub, JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSBool getData(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	TextNode* self = getJsBindable<TextNode>(cx, obj);
	*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, self->data.c_str()));
	return JS_TRUE;
}

static JSBool setData(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	TextNode* self = getJsBindable<TextNode>(cx, obj);

	JsString str(cx, *vp);
	if(!str) return JS_FALSE;

	self->data = str.c_str();

	return JS_TRUE;
}

static JSPropertySpec properties[] = {
	{"data", 0, JsBindable::jsPropFlags, getData, setData},
	{0}
};

TextNode::TextNode(Rhinoca* rh)
	: Node(rh)
	, lineNumber(0)
{
}

TextNode::~TextNode()
{
}

void TextNode::bind(JSContext* cx, JSObject* parent)
{
	ASSERT(!jsContext);
	jsContext = cx;
	jsObject = JS_NewObject(cx, &jsClass, Node::createPrototype(), parent);
	VERIFY(JS_SetPrivate(cx, *this, this));
//	VERIFY(JS_DefineFunctions(cx, *this, methods));
	VERIFY(JS_DefineProperties(cx, *this, properties));
	addReference();	// releaseReference() in JsBindable::finalize()
}

}	// namespace Dom
