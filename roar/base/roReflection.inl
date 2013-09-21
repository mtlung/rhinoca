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

typedef roStatus (*SerializeFunc)(Serializer& se, Type* type, void* val);
roStatus Serialize_int8(Serializer& se, Type* type, void* val);
roStatus Serialize_uint8(Serializer& se, Type* type, void* val);
roStatus Serialize_float(Serializer& se, Type* type, void* val);
roStatus Serialize_generic(Serializer& se, Type* type, void* val);

template<class T>	SerializeFunc	getSerializeFunc(T*)			{ return Serialize_generic; }
inline				SerializeFunc	getSerializeFunc(float*)		{ return Serialize_float; }

template<class T, class C> Type*	extractMemberType(T C::*m)		{ return reflection.getType<T>(); }
template<class T, class C> Type*	extractMemberType(T* C::*m)		{ return reflection.getType<T>(); }
template<class T, class C> bool		isMemberPointerType(T C::*m)	{ return false; }
template<class T, class C> bool		isMemberPointerType(T* C::*m)	{ return true; }
template<class T, class C> bool		isMemberConst(T C::*m)			{ return false; }
template<class T, class C> bool		isMemberConst(const T C::*m)	{ return true; }

template<class T>
struct Klass
{
	template<class F>
	Klass& field(const char* name, F f)
	{
		unsigned offset = reinterpret_cast<unsigned>(&((T*)(NULL)->*f));
		Field tmp = {
			name,
			extractMemberType(f),
			offset,
			isMemberConst(f),
			isMemberPointerType(f)
		};
		type->fields.pushBack(tmp); // TODO: fields better be sorted according to the offset
		return *this;
	}

	Type* type;
};

template<class T>
Klass<T> Registry::Class(const char* name)
{
	Type* type = new Type(name, NULL, &typeid(T));
	type->serializeFunc = getSerializeFunc((T*)NULL);

	types.pushBack(*type);
	Klass<T> k = { type };
	return k;
}

template<class T, class P>
Klass<T> Registry::Class(const char* name)
{
	Type* parent = getType<P>();
	assert(parent);
	Type* type = new Type(name, parent, &typeid(T));
	type->serializeFunc = getSerializeFunc((T*)NULL);

	types.pushBack(*type);
	Klass<T> k = { type };
	return k;
}

template<class T>
Type* Registry::getType()
{
	return getType(typeid(T));
}
