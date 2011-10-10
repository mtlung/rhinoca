#include "pch.h"
#include "cssstyledeclaration.h"
#include "cssstylesheet.h"

namespace Dom {

JSClass CSSStyleDeclaration::jsClass = {
	"CSSStyleDeclaration", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub,
	JS_ConvertStub, JsBindable::finalize, JSCLASS_NO_OPTIONAL_MEMBERS
};

CSSStyleDeclaration::CSSStyleDeclaration()
	: parentRule(NULL)
{
}

CSSStyleDeclaration::~CSSStyleDeclaration()
{
}

void CSSStyleDeclaration::bind(JSContext* cx, JSObject* parent)
{
	RHASSERT(!jsContext);
	jsContext = cx;
	jsObject = JS_NewObject(cx, &jsClass, NULL, parent);
	RHVERIFY(JS_SetPrivate(cx, *this, this));
//	RHVERIFY(JS_DefineFunctions(cx, *this, methods));
//	RHVERIFY(JS_DefineProperties(cx, *this, properties));
	addReference();	// releaseReference() in JsBindable::finalize()
}

}	// namespace Dom
