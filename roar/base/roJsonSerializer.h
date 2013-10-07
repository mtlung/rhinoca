#ifndef __roJsonSerializer_h__
#define __roJsonSerializer_h__

#include "roJson.h"
#include "roReflection.h"

namespace ro {
namespace Reflection {

struct JsonOutputSerializer : public Serializer
{
	JsonOutputSerializer() { isReading = false; }
	override roStatus	beginArchive	();
	override roStatus	endArchive		();
	override roStatus	serialize_int8	(Field& field, void* fieldParent);
	override roStatus	serialize_uint8	(Field& field, void* fieldParent);
	override roStatus	serialize_float	(Field& field, void* fieldParent);
	override roStatus	serialize_double(Field& field, void* fieldParent);
	override roStatus	serialize_string(Field& field, void* fieldParent);
	override roStatus	serialize_object(Field& field, void* fieldParent);
	override roStatus	beginArray		(Field& field, roSize count);
	override roStatus	endArray		();

	override roStatus	serialize		(float& val);

// Writer interface
	override roStatus	serialize		(const roUtf8* val);

// Reader interface
	override bool		isArrayEnded	();

	JsonWriter writer;
};	// JsonOutputSerializer

struct JsonInputSerializer : public Serializer
{
	JsonInputSerializer() { isReading = true; }
	override roStatus	beginArchive	();
	override roStatus	endArchive		();
	override roStatus	serialize_int8	(Field& field, void* fieldParent);
	override roStatus	serialize_uint8	(Field& field, void* fieldParent);
	override roStatus	serialize_float	(Field& field, void* fieldParent);
	override roStatus	serialize_double(Field& field, void* fieldParent);
	override roStatus	serialize_string(Field& field, void* fieldParent);
	override roStatus	serialize_object(Field& field, void* fieldParent);
	override roStatus	beginArray		(Field& field, roSize count);
	override roStatus	endArray		();

	override roStatus	serialize		(float& val);

// Writer interface
	override roStatus	serialize		(const roUtf8* val)	{ return roStatus::not_supported; }

// Reader interface
	override bool		isArrayEnded	();

	roStatus			checkName		(Field& field);

	JsonParser parser;
};	// JsonInputSerializer

}	// namespace Reflection
}   // namespace ro

#endif	// __roJsonSerializer_h__