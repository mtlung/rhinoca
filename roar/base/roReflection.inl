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

typedef roStatus (*ReflectionSerializeFunc)(RSerializer& se, Type* type, void* val);
roStatus reflectionSerialize_int8(RSerializer& se, Type* type, void* val);
roStatus reflectionSerialize_uint8(RSerializer& se, Type* type, void* val);
roStatus reflectionSerialize_float(RSerializer& se, Type* type, void* val);
roStatus reflectionSerialize_generic(RSerializer& se, Type* type, void* val);

template<class T>	ReflectionSerializeFunc reflectionGetSerializeFunc(T*) { return reflectionSerialize_generic; }
inline				ReflectionSerializeFunc reflectionGetSerializeFunc(float*) { return reflectionSerialize_float; }

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
	type->serializeFunc = reflectionGetSerializeFunc((T*)NULL);

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
	type->serializeFunc = reflectionGetSerializeFunc((T*)NULL);

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
