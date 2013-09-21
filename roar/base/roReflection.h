#ifndef __roReflection_h__
#define __roReflection_h__

#include "roArray.h"
#include "roString.h"
#include "roLinkList.h"
#include "roNonCopyable.h"
#include "roJson.h"
#include <typeinfo>

namespace ro {
namespace Reflection {

struct Type;

struct Serializer
{
	Serializer() : _name(NULL) {}
	virtual roStatus serialize_float(void* val) = 0;
	virtual roStatus serialize_string(void* val) = 0;
	virtual void beginStruct(const char* name) = 0;
	virtual void endStruct(const char* name) = 0;
	void beginNVP(const char* name) { _name = name; }
	void endNVP(const char* name) {}

	const char* _name;
};

struct JsonSerializer : public Serializer
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
	roStatus serialize(Serializer& se, void* val);

	String name;
	Type* parent;
	const std::type_info* typeInfo;
	Array<Field> fields;
	Array<Func> funcs;
	roStatus (*serializeFunc)(Serializer& se, Type* type, void* val);
};

template<class T> struct Klass;

struct Registry
{
	template<class T>
	Klass<T> Class(const char* name);

	template<class T, class P>
	Klass<T> Class(const char* name);

	template<class T>
	Type* getType();

	Type* getType(const char* name);
	Type* getType(const std::type_info& typeInfo);

	void reset();

	typedef LinkList<Type> Types;
	Types types;
};

static Registry reflection;

#include "roReflection.inl"

}	// namespace Reflection
}   // namespace ro

#endif	// __roReflection_h__
