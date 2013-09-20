#include "pch.h"
#include "roReflection.h"
#include "roJson.h"

namespace ro {

Type::Type(const char* name, Type* parent, const std::type_info* typeInfo)
	: name(name), parent(parent), typeInfo(typeInfo)
{
	if(parent) {
		fields = parent->fields;
		funcs = parent->funcs;
	}
}

Type* Reflection::getType(const char* name)
{
	Types& types = reflection.types;
	for(Type* i = types.begin(); i != types.end(); i = i->next())
		if(i->name == name)
			return &*i;
	return NULL;
}

Field* Type::getField(const char* name)
{
	for(Field* i = fields.begin(); i != fields.end(); ++i)
		if(i->name == name)
			return &(*i);
	return NULL;
}

struct JsonSerializer
{
	JsonWriter writer;
};

roStatus Field::serialize(Serializer& se)
{
	if(!type) return roStatus::pointer_is_null;

	if(type->fields.isEmpty()) {

	}
	else {

	}

	return roStatus::ok;
}

void Reflection::reset()
{
	types.destroyAll();
}

}   // namespace ro
