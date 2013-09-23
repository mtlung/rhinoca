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

typedef roStatus (*SerializeFunc)(Serializer& se, Field& field, void* fieldParent);
roStatus serialize_int8(Serializer& se, Field& field, void* fieldParent);
roStatus serialize_uint8(Serializer& se, Field& field, void* fieldParent);
roStatus serialize_float(Serializer& se, Field& field, void* fieldParent);
roStatus serialize_double(Serializer& se, Field& field, void* fieldParent);
roStatus serialize_string(Serializer& se, Field& field, void* fieldParent);
roStatus serialize_generic(Serializer& se, Field& field, void* fieldParent);

template<class T>	SerializeFunc	getSerializeFunc(T*)					{ return serialize_generic; }
inline				SerializeFunc	getSerializeFunc(roInt8*)				{ return serialize_int8; }
inline				SerializeFunc	getSerializeFunc(roUint8*)				{ return serialize_uint8; }
inline				SerializeFunc	getSerializeFunc(float*)				{ return serialize_float; }
inline				SerializeFunc	getSerializeFunc(double*)				{ return serialize_double; }
inline				SerializeFunc	getSerializeFunc(char**)				{ return serialize_string; }

template<class T, class C>	Type*	extractMemberType(T C::*m)				{ return reflection.getType<T>(); }
template<class T, class C>	Type*	extractMemberType(T* C::*m)				{ return reflection.getType<T>(); }		// Most of the time we don't care the type is pointer or not
template<class C>			Type*	extractMemberType(char* C::*m)			{ return reflection.getType<char*>(); }
template<class C>			Type*	extractMemberType(const char* C::*m)	{ return reflection.getType<char*>(); }
template<class T, class C>	bool	isMemberPointerType(T C::*m)			{ return false; }
template<class T, class C>	bool	isMemberPointerType(T* C::*m)			{ return true; }
template<class T, class C>	bool	isMemberConst(T C::*m)					{ return false; }
template<class T, class C>	bool	isMemberConst(const T C::*m)			{ return true; }

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
		roAssert("Type not found when registering member field" && tmp.type);
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
