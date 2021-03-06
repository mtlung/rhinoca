#ifndef __roReflection_h__
#define __roReflection_h__

#include "roReflectionFwd.h"
#include "roArray.h"
#include "roString.h"
#include "roLinkList.h"
#include "roNonCopyable.h"
#include "roJson.h"
#include <typeinfo>

namespace ro {
namespace Reflection {

struct Serializer
{
	virtual roStatus beginArchive		() = 0;
	virtual roStatus endArchive			() = 0;
	virtual roStatus serialize_bool		(Field& field, void* fieldParent) = 0;
	virtual roStatus serialize_int8		(Field& field, void* fieldParent) = 0;
	virtual roStatus serialize_int16	(Field& field, void* fieldParent) = 0;
	virtual roStatus serialize_int32	(Field& field, void* fieldParent) = 0;
	virtual roStatus serialize_int64	(Field& field, void* fieldParent) = 0;
	virtual roStatus serialize_uint8	(Field& field, void* fieldParent) = 0;
	virtual roStatus serialize_uint16	(Field& field, void* fieldParent) = 0;
	virtual roStatus serialize_uint32	(Field& field, void* fieldParent) = 0;
	virtual roStatus serialize_uint64	(Field& field, void* fieldParent) = 0;
	virtual roStatus serialize_float	(Field& field, void* fieldParent) = 0;
	virtual roStatus serialize_double	(Field& field, void* fieldParent) = 0;
	virtual roStatus serialize_string	(Field& field, void* fieldParent) = 0;
	virtual roStatus serialize_object	(Field& field, void* fieldParent) = 0;
	virtual roStatus beginArray			(Field& field, roSize count) = 0;
	virtual roStatus endArray			() = 0;

	virtual roStatus serialize			(float& val) = 0;
	virtual roStatus serialize			(double& val) = 0;
	virtual roStatus serialize			(const roUtf8*& val) = 0;
	virtual roStatus serialize			(roByte*& val, roSize& size) = 0;

// Array specialization for better performance
	virtual roStatus serialize			(float* valArray, roSize inoutCount) = 0;

// Reader interface
	// For reader to poll if the array is ended
	virtual bool	 isArrayEnded		() = 0;

	bool isReading;
};

struct Enum
{
	String name;
	int value;
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

	Registry*	getRegistry	();	// The registry which contain this Type
	Field*		getField	(const char* name);
	roStatus	serialize	(Serializer& se, const roUtf8* name, void* val);

	String name;
	Type* parent;
	const std::type_info* typeInfo;
	Array<Type*> templateArgTypes;
	Array<Field> fields;
	Array<Func> funcs;
	roStatus (*serializeFunc)(Serializer& se, Field& field, void* fieldParent);
};

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
