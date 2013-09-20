#ifndef __roReflection_h__
#define __roReflection_h__

#include "roArray.h"
#include "roString.h"
#include "roLinkList.h"
#include "roNonCopyable.h"
#include <typeinfo>

namespace ro {

struct Type;

struct Serializer;

struct Field
{
	void* getPtr(void* self);
	const void* getConstPtr(const void* self) const;

	template<class T>
	T* getPtr(void* self);

	template<class T>
	const T* getConstPtr(const void* self) const;

	roStatus serialize(Serializer& se);

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

	String name;
	Type* parent;
	const std::type_info* typeInfo;
	Array<Field> fields;
	Array<Func> funcs;
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

inline void* Field::getPtr(void* self)
{
	if(isConst) return NULL;
	return ((char*)self) + offset;
}

inline const void* Field::getConstPtr(const void* self) const
{
	return ((char*)self) + offset;
}

template<class T>
T* Field::getPtr(void* self)
{
	if(isConst || !type || typeid(T) != *type->typeInfo)
		return NULL;
	return (T*)(((char*)self) + offset);
}

template<class T>
const T* Field::getConstPtr(const void* self) const
{
	if(!type || typeid(T) != *type->typeInfo)
		return NULL;
	return (T*)(((char*)self) + offset);
}

template<class T, class C> Type*	reflectionExtractMemberType(T C::*m)	{ return reflection.getType<T>(); }
template<class T, class C> Type*	reflectionExtractMemberType(T* C::*m)	{ return reflection.getType<T>(); }
template<class T, class C> bool		reflectionIsMemberPointerType(T C::*m)	{ return false; }
template<class T, class C> bool		reflectionIsMemberPointerType(T* C::*m)	{ return true; }
template<class T, class C> bool		reflectionIsMemberConst(T C::*m)		{ return false; }
template<class T, class C> bool		reflectionIsMemberConst(const T C::*m)	{ return true; }

template<class T>
struct Klass
{
	template<class F>
	Klass& field(const char* name, F f)
	{
		unsigned offset = reinterpret_cast<unsigned>(&((T*)(NULL)->*f));
		Field tmp = {
			name,
			reflectionExtractMemberType(f),
			offset,
			reflectionIsMemberConst(f),
			reflectionIsMemberPointerType(f)
		};
		type->fields.pushBack(tmp); // TODO: fields better be sorted according to the offset
		return *this;
	}

	Type* type;
};

template<class T>
Klass<T> Reflection::Class(const char* name)
{
	Type* type = new Type(name, NULL, &typeid(T));
	types.pushBack(*type);
	Klass<T> k = { type };
	return k;
}

template<class T, class P>
Klass<T> Reflection::Class(const char* name)
{
	Type* parent = getType<P>();
	assert(parent);
	Type* type = new Type(name, parent, &typeid(T));
	types.pushBack(*type);
	Klass<T> k = { type };
	return k;
}

template<class T>
Type* Reflection::getType()
{
	Types& types = reflection.types;
	for(Type* i = types.begin(); i != types.end(); i = i->next())
		if(*i->typeInfo == typeid(T))
			return i;
	return NULL;
}

}   // namespace ro

#endif	// __roReflection_h__
