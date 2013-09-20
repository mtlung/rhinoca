#ifndef __roReflection_h__
#define __roReflection_h__

#include "roArray.h"
#include "roString.h"
#include "roLinkList.h"
#include "roNonCopyable.h"
#include "roJson.h"
#include <typeinfo>

namespace ro {

struct Type;

struct RSerializer
{
	RSerializer() : _name(NULL) {}
	virtual roStatus serialize_float(void* val) = 0;
	virtual roStatus serialize_string(void* val) = 0;
	virtual void beginStruct(const char* name) = 0;
	virtual void endStruct(const char* name) = 0;
	void beginNVP(const char* name) { _name = name; }
	void endNVP(const char* name) {}

	const char* _name;
};

struct JsonRSerializer : public RSerializer
{
	override roStatus serialize_float(void* val);
	override roStatus serialize_string(void* val);
	override void beginStruct(const char* name);
	override void endStruct(const char* name);
	JsonWriter writer;
};

struct Field
{
	void* getPtr(void* self);
	const void* getConstPtr(const void* self) const;

	template<class T>
	T* getPtr(void* self);

	template<class T>
	const T* getConstPtr(const void* self) const;

	String name;
	Type* type;
	unsigned offset;
	bool isConst;
	bool isPointer;
};

struct Func
{
	String name;
};

struct Type : public ListNode<Type>
{
	Type(const char* name, Type* parent, const std::type_info* typeInfo);

	Field* getField(const char* name);
	roStatus serialize(RSerializer& se, void* val);

	String name;
	Type* parent;
	const std::type_info* typeInfo;
	Array<Field> fields;
	Array<Func> funcs;
	roStatus (*serializeFunc)(RSerializer& se, Type* type, void* val);
};

template<class T> struct Klass;

struct Reflection
{
	template<class T>
	Klass<T> Class(const char* name);

	template<class T, class P>
	Klass<T> Class(const char* name);

	template<class T>
	Type* getType();

	Type* getType(const char* name);

	void reset();

	typedef LinkList<Type> Types;
	Types types;
};

static Reflection reflection;

#include "roReflection.inl"

}   // namespace ro

#endif	// __roReflection_h__
