#include "pch.h"
#include "roReflection.h"
#include "roJson.h"

namespace ro {
namespace Reflection {

Registry reflection;

Type::Type(const char* name, Type* parent, const std::type_info* typeInfo)
	: name(name), parent(parent), typeInfo(typeInfo), serializeFunc(NULL)
{
	if(parent) {
		fields = parent->fields;
		funcs = parent->funcs;
	}
}

Type* Registry::getType(const char* name)
{
	for(Type* i=types.begin(), *end=types.end(); i != end; i = i->next())
		if(i->name == name)
			return &*i;
	return NULL;
}

Type* Registry::getType(const std::type_info& typeInfo)
{
	for(Type* i = types.begin(); i != types.end(); i = i->next())
		if(*i->typeInfo == typeInfo)
			return i;
	return NULL;
}

Registry* Type::getRegistry()
{
	return roContainerof(Registry, types, getList());
}

Field* Type::getField(const char* name_)
{
	for(Field* i=fields.begin(), *end=fields.end(); i != end; ++i)
		if(i->name == name_)
			return &(*i);
	return NULL;
}

roStatus Type::serialize(Serializer& se, const roUtf8* varName, void* val)
{
	if(!serializeFunc || !val) return roStatus::pointer_is_null;
	if(!varName) varName = "";
	// Make a dummy field as root
	Field field = { varName, this, 0, false, false };
	return (*serializeFunc)(se, field, val);
}

roStatus serialize_int8(Serializer& se, Field& field, void* fieldParent)
{
	return se.serialize_int8(field, fieldParent);
}

roStatus serialize_uint8(Serializer& se, Field& field, void* fieldParent)
{
	return se.serialize_uint8(field, fieldParent);
}

roStatus serialize_float(Serializer& se, Field& field, void* fieldParent)
{
	return se.serialize_float(field, fieldParent);
}

roStatus serialize_double(Serializer& se, Field& field, void* fieldParent)
{
	return se.serialize_double(field, fieldParent);
}

roStatus serialize_string(Serializer& se, Field& field, void* fieldParent)
{
	return se.serialize_string(field, fieldParent);
}

// Serialization of object goes here, if not been customized
roStatus serialize_generic(Serializer& se, Field& field, void* fieldParent)
{
	return se.serialize_object(field, fieldParent);
}

void Registry::reset()
{
	types.destroyAll();
}

}	// namespace Reflection
}   // namespace ro
