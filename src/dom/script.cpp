#include "pch.h"
#include "script.h"
#include "textnode.h"

namespace Dom {

JSClass HTMLScriptElement::jsClass = {
	"HTMLScriptElement", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub, JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSBool getSrc(JSContext* cx, JSObject* obj, jsid id, jsval* vp)
{
	HTMLScriptElement* self = getJsBindable<HTMLScriptElement>(cx, obj);
	*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, self->src().c_str()));
	return JS_TRUE;
}

static JSBool setSrc(JSContext* cx, JSObject* obj, jsid id, JSBool strict, jsval* vp)
{
	HTMLScriptElement* self = getJsBindable<HTMLScriptElement>(cx, obj);

	JsString str(cx, *vp);
	if(!str) return JS_FALSE;

	self->setSrc(str.c_str());

	return JS_TRUE;
}

static JSPropertySpec properties[] = {
	{"src", 0, JsBindable::jsPropFlags, getSrc, setSrc},
	{0}
};

HTMLScriptElement::HTMLScriptElement(Rhinoca* rh)
	: Element(rh)
	, inited(false)
{
}

HTMLScriptElement::~HTMLScriptElement()
{
}

void HTMLScriptElement::bind(JSContext* cx, JSObject* parent)
{
	ASSERT(!jsContext);
	jsContext = cx;
	jsObject = JS_NewObject(cx, &jsClass, Element::createPrototype(), parent);
	VERIFY(JS_SetPrivate(cx, *this, this));
//	VERIFY(JS_DefineFunctions(cx, *this, methods));
	VERIFY(JS_DefineProperties(cx, *this, properties));
	addReference();	// releaseReference() in JsBindable::finalize()
}

void HTMLScriptElement::onParserEndElement()
{
	TextNode* text = dynamic_cast<TextNode*>(firstChild);
	if(!text) return;
}

static const FixString _tagName = "SCRIPT";

Element* HTMLScriptElement::factoryCreate(Rhinoca* rh, const char* type, XmlParser* parser)
{
	return strcasecmp(type, _tagName) == 0 ? new HTMLScriptElement(rh) : NULL;
}

void HTMLScriptElement::setSrc(const char* uri)
{
	// We only allow to set the source once
	if(inited) return;
	_src = uri;
}

const FixString& HTMLScriptElement::src() const
{
	return _src;
}

const FixString& HTMLScriptElement::tagName() const
{
	return _tagName;
}

}	// namespace Dom
