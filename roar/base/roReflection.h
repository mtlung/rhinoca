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
struct Field;

struct Serializer
{
	virtual roStatus serialize_int8(Field& field, void* fieldParent) = 0;
	virtual roStatus serialize_uint8(Field& field, void* fieldParent) = 0;
	virtual roStatus serialize_float(Field& field, void* fieldParent) = 0;
	virtual roStatus serialize_double(Field& field, void* fieldParent) = 0;
	virtual roStatus serialize_string(Field& field, void* fieldParent) = 0;
	virtual roStatus serialize_object(Field& field, void* fieldParent) = 0;
	virtual roStatus serialize(float val) = 0;
	virtual roStatus serialize(const roUtf8* val) = 0;
	virtual roStatus beginArray(Field& field, roSize count) = 0;
	virtual roStatus endArray() = 0;
};

struct JsonSerializer : public Serializer
{
	override roStatus serialize_int8(Field& field, void* fieldParent);
	override roStatus serialize_uint8(Field& field, void* fieldParent);
	override roStatus serialize_float(Field& field, void* fieldParent);
	override roStatus serialize_double(Field& field, void* fieldParent);
	override roStatus serialize_string(Field& field, void* fieldParent);
	override roStatus serialize_object(Field& field, void* fieldParent);
	override roStatus serialize(float val);
	override roStatus serialize(const roUtf8* val);
	override roStatus beginArray(Field& field, roSize count);
	override roStatus endArray();
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
	roStatus serialize(Serializer& se, const roUtf8* name, void* val);

	String name;
	Type* parent;
	const std::type_info* typeInfo;
	Array<Field> fields;
	Array<Func> funcs;
	roStatus (*serializeFunc)(Serializer& se, Field& field, void* fieldParent);
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

extern Registry reflection;

#include "roReflection.inl"

}	// namespace Reflection
}   // namespace ro

#endif	// __roReflection_h__
