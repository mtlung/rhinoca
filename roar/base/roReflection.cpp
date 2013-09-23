#include "pch.h"
#include "roReflection.h"
#include "roJson.h"

namespace ro {
namespace Reflection {

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

Field* Type::getField(const char* name)
{
	for(Field* i=fields.begin(), *end=fields.end(); i != end; ++i)
		if(i->name == name)
			return &(*i);
	return NULL;
}

roStatus Type::serialize(Serializer& se, const roUtf8* name, void* val)
{
	if(!serializeFunc || !val) return roStatus::pointer_is_null;
	if(!name) name = "";
	// Make a dummy field as root
	Field field = { name, this, 0, false, false };
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

roStatus serialize_generic(Serializer& se, Field& field, void* fieldParent)
{
	return se.serialize_object(field, fieldParent);
}

void Registry::reset()
{
	types.destroyAll();
}

roStatus JsonSerializer::serialize_int8(Field& field, void* fieldParent)
{
	if(!fieldParent) return roStatus::pointer_is_null;
	return writer.write(field.name.c_str(), *reinterpret_cast<const roInt8*>(field.getConstPtr(fieldParent)));
}

roStatus JsonSerializer::serialize_uint8(Field& field, void* fieldParent)
{
	if(!fieldParent) return roStatus::pointer_is_null;
	return writer.write(field.name.c_str(), *reinterpret_cast<const roUint8*>(field.getConstPtr(fieldParent)));
}

roStatus JsonSerializer::serialize_float(Field& field, void* fieldParent)
{
	if(!fieldParent) return roStatus::pointer_is_null;
	return writer.write(field.name.c_str(), *reinterpret_cast<const float*>(field.getConstPtr(fieldParent)));
}

roStatus JsonSerializer::serialize_double(Field& field, void* fieldParent)
{
	if(!fieldParent) return roStatus::pointer_is_null;
	return writer.write(field.name.c_str(), *reinterpret_cast<const double*>(field.getConstPtr(fieldParent)));
}

roStatus JsonSerializer::serialize_string(Field& field, void* fieldParent)
{
	if(!fieldParent) return roStatus::pointer_is_null;
	return writer.write(field.name.c_str(), *(const char**)(field.getConstPtr(fieldParent)));
}

roStatus JsonSerializer::serialize_object(Field& field, void* fieldParent)
{
	Type* type = field.type;
	if(!type) return roStatus::pointer_is_null;

	writer.beginObject(field.name.c_str());

	for(Field* f=type->fields.begin(), *end=type->fields.end(); f != end; ++f) {
		if(!f->type) return roStatus::pointer_is_null;
		roStatus st = f->type->serializeFunc(*this, *f, fieldParent);
		if(!st) return st;
	}

	writer.endObject();

	return roStatus::ok;
}

}	// namespace Reflection
}   // namespace ro
