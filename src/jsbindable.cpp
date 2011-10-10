#include "pch.h"
#include "jsbindable.h"
#include "rhinoca.h"

#ifdef DEBUG
#	include <typeinfo>	// For debugging GC problem
#endif

JsBindable::JsBindable()
	: jsContext(NULL)
	, jsObject(NULL)
	, refCount(0)
	, gcRootCount(0)
{}

JsBindable::~JsBindable()
{
	RHASSERT(refCount == 0);
	RHASSERT(gcRootCount == 0);
//	RHASSERT(!jsContext);
//	RHASSERT(!jsObject);
}

void JsBindable::addGcRoot()
{
	if(++gcRootCount > 1)
		return;

	addReference();	// releaseReference() in JsBindable::releaseGcRoot()
	RHASSERT(jsContext);
	if(typeName.empty()) {
#ifdef DEBUG
		RHVERIFY(JS_AddNamedObjectRoot(jsContext, &jsObject, typeid(*this).name()));
#else
		RHVERIFY(JS_AddObjectRoot(jsContext, &jsObject));
#endif
	}
	else
		RHVERIFY(JS_AddNamedObjectRoot(jsContext, &jsObject, typeName.c_str()));
}

void JsBindable::releaseGcRoot()
{
	if(--gcRootCount > 0)
		return;

	RHASSERT(jsContext);
	RHVERIFY(JS_RemoveObjectRoot(jsContext, &jsObject));
	releaseReference();
}

void JsBindable::addReference()
{
	++refCount;
}

void JsBindable::releaseReference()
{
	RHASSERT(refCount > 0);
	--refCount;
	if(refCount > 0) return;
	delete this;
}

JsBindable::operator jsval()
{
	if(!this) return JSVAL_NULL;
	return jsObject ? OBJECT_TO_JSVAL(jsObject) : JSVAL_NULL;
}

JSObject* JsBindable::jsObjectOfType(JSClass* c)
{
	JSObject* obj = jsObject;

	while(obj && JS_GetClass(obj) != c)
		obj = JS_GetPrototype(jsContext, obj);

	return obj;
}

void* JsBindable::operator new(size_t size)
{
	return rhinoca_malloc(size);
}

void JsBindable::operator delete(void* ptr)
{
	rhinoca_free(ptr);
}

void JsBindable::finalize(JSContext* cx, JSObject* obj)
{
	if(JsBindable* p = reinterpret_cast<JsBindable*>(JS_GetPrivate(cx, obj)))
		p->releaseReference();

//	p->jsContext = NULL;
//	p->jsObject = NULL;
}

JsString::JsString(JSContext* cx, jsval v)
	: _bytes(NULL), _length(0)
{
	if(JSString* jss = JS_ValueToString(cx, v)) {
		_bytes = JS_EncodeString(cx, jss);
		_length = JS_GetStringEncodingLength(cx, jss);
	}
}

JsString::JsString(JSContext* cx, jsval* vp, unsigned paramIdx)
	: _bytes(NULL), _length(0)
{
	if(JSString* jss = JS_ValueToString(cx, JS_ARGV(cx, vp)[paramIdx])) {
		_bytes = JS_EncodeString(cx, jss);
		_length = JS_GetStringEncodingLength(cx, jss);
	}
}

JsString::~JsString()
{
	js_free(_bytes);
}

JsBindable* getJsBindable(JSContext* cx, JSObject* obj, JSClass* jsClass, jsval* argv)
{
	void* ret = JS_GetInstancePrivate(cx, obj, jsClass, argv);

	while(!ret && obj) {
		obj = JS_GetPrototype(cx, obj);
		ret = JS_GetInstancePrivate(cx, obj, jsClass, argv);
	}

	if(ret) JS_ClearPendingException(cx);

	return reinterpret_cast<JsBindable*>(ret);
}

JsBindable* getJsBindableExactType(JSContext* cx, JSObject* obj, JSClass* jsClass, jsval* argv)
{
	void* ret = JS_GetInstancePrivate(cx, obj, jsClass, argv);
	return reinterpret_cast<JsBindable*>(ret);
}


void print(JSContext* cx, const char* format, ...)
{
	if(!format) return;

	Rhinoca* rh = reinterpret_cast<Rhinoca*>(JS_GetContextPrivate(cx));
	va_list vl;
	va_start(vl, format);
	vprint(rh, format, vl);
	va_end(vl);
}

JSBool JS_GetValue(JSContext *cx, jsval jv, bool& val)
{
	JSBool b;
	if(JSVAL_IS_BOOLEAN(jv))
		b = JSVAL_TO_BOOLEAN(jv);
	else if(JSVAL_IS_STRING(jv)) {
		JsString jss(cx, jv);
		b = (strcasecmp(jss.c_str(), "false") != 0);
	}
	else if(JS_ValueToBoolean(cx, jv, &b) == JS_FALSE)
		return JS_FALSE;

	val = (b == JS_TRUE);
	return JS_TRUE;
}

JSBool JS_GetValue(JSContext *cx, jsval jv, int& val)
{
	int32 v;
	if(JS_ValueToInt32(cx, jv, &v) == JS_FALSE)
		return JS_FALSE;
	val = v;
	return JS_TRUE;
}

JSBool JS_GetValue(JSContext *cx, jsval jv, unsigned& val)
{
	uint32 v;
	if(JS_ValueToECMAUint32(cx, jv, &v) == JS_FALSE)
		return JS_FALSE;
	val = v;
	return JS_TRUE;
}

JSBool JS_GetValue(JSContext *cx, jsval jv, float& val)
{
	jsdouble v;
	if(JS_ValueToNumber(cx, jv, &v) == JS_FALSE)
		return JS_FALSE;
	val = (float)v;
	return JS_TRUE;
}

JSBool JS_GetValue(JSContext *cx, jsval jv, FixString& val)
{
	JsString jss(cx, jv);
	if(!jss) return JS_FALSE;

	val = jss.c_str();
	return JS_TRUE;
}
