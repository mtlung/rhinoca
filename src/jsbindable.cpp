#include "pch.h"
#include "jsbindable.h"
#include "common.h"

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
	if(typeName.empty())
		VERIFY(JS_AddRoot(jsContext, &jsObject));
	else
		VERIFY(JS_AddNamedRoot(jsContext, &jsObject, typeName.c_str()));
}

void JsBindable::releaseGcRoot()
{
	if(--gcRootCount > 0)
		return;

	ASSERT(jsContext);
	VERIFY(JS_RemoveRoot(jsContext, &jsObject));
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
	return OBJECT_TO_JSVAL(jsObject ? jsObject : JSVAL_NULL);
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
	if(JsBindable* p = reinterpret_cast<JsBindable*>(JS_GetPrivate(cx, obj))) {
		p->releaseReference();
	}
//	p->jsContext = NULL;
//	p->jsObject = NULL;
}
