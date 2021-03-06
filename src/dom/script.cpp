#include "pch.h"
#include "script.h"
#include "textnode.h"
#include "../common.h"
#include "../context.h"
#include "../path.h"
#include "../../roar/base/roFileSystem.h"
#include "../../roar/base/roTextResource.h"

namespace ro {
extern bool resourceLoadText(ResourceManager*, Resource*);
}

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
	*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, self->src()));
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

static JSBool construct(JSContext* cx, uintN argc, jsval* vp)
{
	if(!JS_IsConstructing(cx, vp)) return JS_FALSE;	// Not called as constructor? (called without new)

	Rhinoca* rh = reinterpret_cast<Rhinoca*>(JS_GetContextPrivate(cx));
	HTMLScriptElement* script = new HTMLScriptElement(rh);
	script->bind(cx, NULL);

	JS_RVAL(cx, vp) = *script;

	return JS_TRUE;
}

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
	roAssert(!jsContext);
	jsContext = cx;
	jsObject = JS_NewObject(cx, &jsClass, Element::createPrototype(), parent);
	roVerify(JS_SetPrivate(cx, *this, this));
//	roVerify(JS_DefineFunctions(cx, *this, methods));
	roVerify(JS_DefineProperties(cx, *this, properties));
	addReference();	// releaseReference() in JsBindable::finalize()
}

void HTMLScriptElement::onParserEndElement()
{
	TextNode* text = dynamic_cast<TextNode*>(firstChild);
	if(!text) return;

	runScript(text->data.c_str(), NULL, text->lineNumber);
}

bool HTMLScriptElement::runScript(const char* code, const char* srcPath, unsigned lineNumber)
{
	unsigned len = strlen(code);

	if(!srcPath)
		srcPath = "unknown source";

	jsval rval;
	JSBool ret = JS_EvaluateScript(jsContext, *rhinoca->domWindow, code, len, srcPath, lineNumber+1, &rval);

	return ret == JS_TRUE;
}

void HTMLScriptElement::registerClass(JSContext* cx, JSObject* parent)
{
	roVerify(JS_InitClass(cx, parent, NULL, &jsClass, &construct, 0, NULL, NULL, NULL, NULL));
}

static const ro::ConstString _tagName = "SCRIPT";

Element* HTMLScriptElement::factoryCreate(Rhinoca* rh, const char* type, XmlParser* parser)
{
	return roStrCaseCmp(type, _tagName) == 0 ? new HTMLScriptElement(rh) : NULL;
}

void HTMLScriptElement::setSrc(const char* uri)
{
	// We only allow to set the source once
	if(inited) return;

	Path path;
	fixRelativePath(uri, rhinoca->documentUrl.c_str(), path);

	ro::ResourceManager& mgr = *rhinoca->subSystems.resourceMgr;

	ro::TextResourcePtr textResource = new ro::TextResource(path.c_str());
	_src = mgr.loadAs<ro::TextResource>(textResource.get(), ro::resourceLoadText);

	// TODO: Put into the task pool instead of a blocking one
	mgr.taskPool->wait(_src->taskLoaded);

	// TODO: Prevent the same script running more than once
	runScript(_src->data.c_str(), path.c_str());

	// TODO: It's a temp solution to reload script, later should use the file monitor
	mgr.forget(path.c_str());
}

const char* HTMLScriptElement::src() const
{
	return _src ?
		(_src->data.isEmpty() ?
			"" : _src->data.c_str()
		) :
		"";
}

const ro::ConstString& HTMLScriptElement::tagName() const
{
	return _tagName;
}

}	// namespace Dom
