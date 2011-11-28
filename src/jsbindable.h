#ifndef __JSBINDABLE_H__
#define __JSBINDABLE_H__

#include "rhassert.h"
#include "../roar/base/roString.h"

#define XP_WIN
#include "../thirdParty/SpiderMonkey/jsapi.h"
#undef XP_WIN

// Reference: http://egachine.berlios.de/embedding-sm-best-practice/embedding-sm-best-practice.html
class JsBindable
{
public:
	JsBindable();
	virtual ~JsBindable();

// Operations
	virtual void bind(JSContext* cx, JSObject* parent) {}

	/// Add the object to the GC root, auto release in ~JsBindable()
	void addGcRoot();
	void releaseGcRoot();

	void addReference();
	void releaseReference();

	operator jsval();
	operator JSObject*() { return this ? jsObject : NULL; }

	/// JsBindable::jsObject store the most derived object, if you want to 
	/// access object in the prototype chain, use this function.
	JSObject* jsObjectOfType(JSClass* c);

	void* operator new(size_t);
	void operator delete(void*);

// Attributes
	JSContext* jsContext;
	JSObject* jsObject;
	int refCount;		/// Governing when the object will be deleted
	int gcRootCount;	/// Control when the Javascript engine will give up ownership
	ro::ConstString typeName;

	static void finalize(JSContext* cx, JSObject* obj);

	/// Common flags for defining js property
	static const unsigned jsPropFlags = JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED;
};	// JsBindable

/// Get JsBindable from JSObject
extern JsBindable* getJsBindable(JSContext* cx, JSObject* obj, JSClass* jsClass, jsval* argvForDebugPrint=NULL);

extern JsBindable* getJsBindableExactType(JSContext* cx, JSObject* obj, JSClass* jsClass, jsval* argvForDebugPrint=NULL);

template<class T>
static T* getJsBindable(JSContext* cx, JSObject* obj)
{
	JsBindable* b = getJsBindable(cx, obj, &T::jsClass);
	return static_cast<T*>(b);
}

template<class T>
static T* getJsBindableExactType(JSContext* cx, JSObject* obj)
{
	JsBindable* b = getJsBindableExactType(cx, obj, &T::jsClass);
	return static_cast<T*>(b);
}

/// Get the 'this' pointer for the current JS function
template<class T>
static T* getJsBindable(JSContext* cx, jsval* vp)
{
	JsBindable* ret = getJsBindable<T>(cx, JS_THIS_OBJECT(cx, vp));
	RHASSERT(dynamic_cast<T*>(ret));
	return static_cast<T*>(ret);
}

/// Get the i-th parameter as JsBindable
template<class T>
static T* getJsBindable(JSContext* cx, jsval* vp, unsigned paramIdx)
{
	return  static_cast<T*>(getJsBindable(cx, JSVAL_TO_OBJECT(JS_ARGV(cx, vp)[paramIdx]), &T::jsClass, JS_ARGV(cx, vp)));
}

template<class T>
static T* getJsBindableNoThrow(JSContext* cx, jsval* vp, unsigned paramIdx)
{
	return getJsBindable<T>(cx, JSVAL_TO_OBJECT(JS_ARGV(cx, vp)[paramIdx]));
}

template<class T>
static T* getJsBindableExactTypeNoThrow(JSContext* cx, jsval* vp, unsigned paramIdx)
{
	return getJsBindableExactType<T>(cx, JSVAL_TO_OBJECT(JS_ARGV(cx, vp)[paramIdx]));
}

class JsString
{
public:
	JsString(JSContext* cx, jsval v);
	JsString(JSContext* cx, jsval* vp, unsigned paramIdx);
	~JsString();

    char* c_str() const { return _bytes; }

	unsigned size() const { return _length; }


	//!	Non-Null test for using "if (p) ..." to check whether p is NULL.
	typedef char* JsString::*unspecified_bool_type;
	operator unspecified_bool_type() const {
		return _bytes == NULL ? NULL : &JsString::_bytes;
	}

	bool operator!() const { return !_bytes; }

private:
	char* _bytes;
	unsigned _length;

	JsString(const JsString&);
	JsString& operator=(const JsString&);
};	// JsString

JSBool JS_GetValue(JSContext *cx, jsval jv, bool& val);
JSBool JS_GetValue(JSContext *cx, jsval jv, int& val);
JSBool JS_GetValue(JSContext *cx, jsval jv, unsigned& val);
JSBool JS_GetValue(JSContext *cx, jsval jv, float& val);
JSBool JS_GetValue(JSContext *cx, jsval jv, ro::ConstString& val);

#define JS_ARGV0 (JS_ARGV(cx, vp)[0])
#define JS_ARGV1 (JS_ARGV(cx, vp)[1])
#define JS_ARGV2 (JS_ARGV(cx, vp)[2])
#define JS_ARGV3 (JS_ARGV(cx, vp)[3])
#define JS_ARGV4 (JS_ARGV(cx, vp)[4])
#define JS_ARGV5 (JS_ARGV(cx, vp)[5])
#define JS_ARGV6 (JS_ARGV(cx, vp)[6])
#define JS_ARGV7 (JS_ARGV(cx, vp)[7])
#define JS_ARGV8 (JS_ARGV(cx, vp)[8])
#define JS_ARGV9 (JS_ARGV(cx, vp)[9])

#endif	// __JSBINDABLE_H__
