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
	const roInt8* val = reinterpret_cast<const roInt8*>(field.getConstPtr(fieldParent));
	return field.name.isEmpty() ? writer.write(*val) : writer.write(field.name.c_str(), *val);
}

roStatus JsonSerializer::serialize_uint8(Field& field, void* fieldParent)
{
	if(!fieldParent) return roStatus::pointer_is_null;
	const roUint8* val = reinterpret_cast<const roUint8*>(field.getConstPtr(fieldParent));
	return field.name.isEmpty() ? writer.write(*val) : writer.write(field.name.c_str(), *val);
}

roStatus JsonSerializer::serialize_float(Field& field, void* fieldParent)
{
	if(!fieldParent) return roStatus::pointer_is_null;
	const float* val = reinterpret_cast<const float*>(field.getConstPtr(fieldParent));
	return field.name.isEmpty() ? writer.write(*val) : writer.write(field.name.c_str(), *val);
}

roStatus JsonSerializer::serialize_double(Field& field, void* fieldParent)
{
	if(!fieldParent) return roStatus::pointer_is_null;
	const double* val = reinterpret_cast<const double*>(field.getConstPtr(fieldParent));
	return field.name.isEmpty() ? writer.write(*val) : writer.write(field.name.c_str(), *val);
}

roStatus JsonSerializer::serialize_string(Field& field, void* fieldParent)
{
	if(!fieldParent) return roStatus::pointer_is_null;
	const char** val = (const char**)(field.getConstPtr(fieldParent));
	return field.name.isEmpty() ? writer.write(*val) : writer.write(field.name.c_str(), *val);
}

roStatus JsonSerializer::serialize_object(Field& field, void* fieldParent)
{
	Type* type = field.type;
	if(!type) return roStatus::pointer_is_null;

	const char* name = field.name.isEmpty() ? NULL : field.name.c_str();
	writer.beginObject(name);

	for(Field* f=type->fields.begin(), *end=type->fields.end(); f != end; ++f) {
		if(!f->type) return roStatus::pointer_is_null;
		roStatus st = f->type->serializeFunc(*this, *f, fieldParent);
		if(!st) return st;
	}

	writer.endObject();

	return roStatus::ok;
}

roStatus JsonSerializer::serialize(float val)
{
	return writer.write(val);
}

roStatus JsonSerializer::serialize(const roUtf8* val)
{
	return writer.write(val);
}

roStatus JsonSerializer::beginArray(Field& field, roSize count)
{
	return writer.beginArray(field.name.isEmpty() ? NULL : field.name.c_str());
}

roStatus JsonSerializer::endArray()
{
	return writer.endArray();
}

}	// namespace Reflection
}   // namespace ro
