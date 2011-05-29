#ifndef __JSBINDABLE_H__
#define __JSBINDABLE_H__

#include "rhstring.h"

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

	void* operator new(size_t);
	void operator delete(void*);

// Attributes
	JSContext* jsContext;
	JSObject* jsObject;
	int refCount;		/// Governing when the object will be deleted
	int gcRootCount;	/// Control when the Javascript engine will giveup ownership
	FixString typeName;

	static void finalize(JSContext* cx, JSObject* obj);
};	// JsBindable

#endif	// __JSBINDABLE_H__
