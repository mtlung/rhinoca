#include "pch.h"
#include "textnode.h"

namespace Dom {

JSClass TextNode::jsClass = {
	"TextNode", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub, JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
};

TextNode::TextNode(Rhinoca* rh)
	: Node(rh)
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
//	VERIFY(JS_DefineProperties(cx, *this, properties));
	addReference();	// releaseReference() in JsBindable::finalize()
}

}	// namespace Dom
