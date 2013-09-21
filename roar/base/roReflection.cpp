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

roStatus Type::serialize(Serializer& se, void* val)
{
	if(!serializeFunc || !val) return roStatus::pointer_is_null;
	return (*serializeFunc)(se, this, val);
}

roStatus Serialize_float(Serializer& se, Type* type, void* val)
{
	return se.serialize_float(val);
}

roStatus Serialize_generic(Serializer& se, Type* type, void* val)
{
	if(!type || !val) return roStatus::pointer_is_null;

	se.beginStruct(se._name);
	for(Field* f=type->fields.begin(), *end=type->fields.end(); f != end; ++f) {
		se.beginNVP(f->name.c_str());
		f->type->serializeFunc(se, f->type, (void*)f->getConstPtr(val));
		se.endNVP(f->name.c_str());
	}
	se.endStruct(se._name);

	return roStatus::ok;
}

void Registry::reset()
{
	types.destroyAll();
}

roStatus JsonSerializer::serialize_float(void* val)
{
	if(!val) return roStatus::pointer_is_null;
	return writer.write(_name, *(float*)val);
}

roStatus JsonSerializer::serialize_string(void* val)
{
	return roStatus::ok;
}

void JsonSerializer::beginStruct(const char* name)
{
	writer.beginObject(name);
}

void JsonSerializer::endStruct(const char* name)
{
	writer.endObject();
}

}	// namespace Reflection
}   // namespace ro
