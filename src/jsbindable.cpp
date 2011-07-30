#include "pch.h"
#include "jsbindable.h"
#include "common.h"

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
	ASSERT(refCount == 0);
	ASSERT(gcRootCount == 0);
//	ASSERT(!jsContext);
//	ASSERT(!jsObject);
}

void JsBindable::addGcRoot()
{
	if(++gcRootCount > 1)
		return;

	addReference();	// releaseReference() in JsBindable::releaseGcRoot()
	ASSERT(jsContext);
	if(typeName.empty()) {
#ifdef DEBUG
		VERIFY(JS_AddNamedObjectRoot(jsContext, &jsObject, typeid(*this).name()));
#else
		VERIFY(JS_AddObjectRoot(jsContext, &jsObject));
#endif
	}
	else
		VERIFY(JS_AddNamedObjectRoot(jsContext, &jsObject, typeName.c_str()));
}

void JsBindable::releaseGcRoot()
{
	if(--gcRootCount > 0)
		return;

	ASSERT(jsContext);
	VERIFY(JS_RemoveObjectRoot(jsContext, &jsObject));
	releaseReference();
}

void JsBindable::addReference()
{
	++refCount;
}

void JsBindable::releaseReference()
{
	ASSERT(refCount > 0);
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

JSBool JS_GetValue(JSContext *cx, jsval jv, bool& val)
{
	if(JS_ValueToBoolean(cx, jv, (JSBool*)&val) == JS_FALSE)
		return JS_FALSE;
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
