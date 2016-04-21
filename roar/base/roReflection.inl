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

template<class T>
struct Klass
{
	template<class F>
	Klass& field(const char* name, F f)
	{
		Field tmp;
		extractField(*this, tmp, name, f, f);
		roAssert("Type not found when registering member field" && tmp.type);
		type->fields.pushBack(tmp); // TODO: fields better be sorted according to the offset
		return *this;
	}

	template<class F>
	Klass& func(const char* name, F f)
	{
		return *this;
	}

	Type* type;
	Registry* registry;
};

template<class T>
Klass<T> Registry::Class(const char* name)
{
	Type* type = getType<T>();
	if(!type) {
		type = new Type(name, NULL, &typeid(T));
		types.pushBack(*type);
	}

	type->serializeFunc = getSerializeFunc((T*)NULL);
	Klass<T> k = { type, this };
	return k;
}

template<class T, class P>
Klass<T> Registry::Class(const char* name)
{
	Type* parent = getType<P>();
	roAssert(parent);
	Type* type = getType<T>();
	if(!type) {
		type = new Type(name, parent, &typeid(T));
		types.pushBack(*type);
	}

	type->serializeFunc = getSerializeFunc((T*)NULL);
	Klass<T> k = { type, this };
	return k;
}

template<class T>
Type* Registry::getType()
{
	return getType(typeid(T));
}


// Below are designed for extending the reflection system

typedef roStatus (*SerializeFunc)(Serializer& se, Field& field, void* fieldParent);
roStatus serialize_int8(Serializer& se, Field& field, void* fieldParent);
roStatus serialize_uint8(Serializer& se, Field& field, void* fieldParent);
roStatus serialize_float(Serializer& se, Field& field, void* fieldParent);
roStatus serialize_double(Serializer& se, Field& field, void* fieldParent);
roStatus serialize_string(Serializer& se, Field& field, void* fieldParent);
roStatus serialize_generic(Serializer& se, Field& field, void* fieldParent);

template<class T>	SerializeFunc	getSerializeFunc(T*)								{ return serialize_generic; }
inline				SerializeFunc	getSerializeFunc(roInt8*)							{ return serialize_int8; }
inline				SerializeFunc	getSerializeFunc(roUint8*)							{ return serialize_uint8; }
inline				SerializeFunc	getSerializeFunc(float*)							{ return serialize_float; }
inline				SerializeFunc	getSerializeFunc(double*)							{ return serialize_double; }
inline				SerializeFunc	getSerializeFunc(char**)							{ return serialize_string; }

template<class T, class C>	Type*	extractMemberType(Registry& r, T C::*m)				{ return r.getType<T>(); }
template<class T, class C>	Type*	extractMemberType(Registry& r, T* C::*m)			{ return r.getType<T>(); }		// Most of the time we don't care the type is pointer or not
template<class C>			Type*	extractMemberType(Registry& r, char* C::*m)			{ return r.getType<char*>(); }
template<class C>			Type*	extractMemberType(Registry& r, const char* C::*m)	{ return r.getType<char*>(); }
template<class T, class C>	bool	isMemberPointerType(T C::*m)						{ return false; }
template<class T, class C>	bool	isMemberPointerType(T* C::*m)						{ return true; }
template<class T, class C>	bool	isMemberConst(T C::*m)								{ return false; }
template<class T, class C>	bool	isMemberConst(const T C::*m)						{ return true; }

template<class T> struct DisableResolution { typedef void Ret; };	// Use this to disable default extractField() when ambiguous happen

template<class T, class F, class C>
void extractField(Klass<C>& klass, Field& outField, const char* name, F f, T C::*m, typename DisableResolution<T>::Ret* dummy=NULL)
{
	Registry* registry = klass.registry;
	roAssert(registry);
	roPtrInt offset = reinterpret_cast<roPtrInt>(&((C*)(NULL)->*f));
	Field tmp = {
		name,
		extractMemberType(*registry, f),
		(unsigned)offset,
		isMemberConst(f),
		isMemberPointerType(f)
	};
	outField = tmp;
}

template<class T>
Type* RegisterTemplateArgument(Registry& registry, T* dummy=NULL)
{
	Type* type = registry.getType<T>();
	roAssert("Type not found when registering" && type);
	return type;
}
