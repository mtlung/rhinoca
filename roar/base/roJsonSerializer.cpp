#include "pch.h"
#include "roJsonSerializer.h"

namespace ro {
namespace Reflection {

roStatus JsonOutputSerializer::beginArchive()
{
	writer.beginDocument();
	return writer.beginObject();
}

roStatus JsonOutputSerializer::endArchive()
{
	roStatus st = writer.endObject();
	if(!st) return st;
	writer.endDocument();
	return st;
}

template<class T>
roStatus _serialize(JsonWriter& writer, Field& field, void* fieldParent)
{
	if(!fieldParent) return roStatus::pointer_is_null;
	const T* val = reinterpret_cast<const T*>(field.getConstPtr(fieldParent));
	return field.name.isEmpty() ? writer.write(*val) : writer.write(field.name.c_str(), *val);
}

roStatus JsonOutputSerializer::serialize_bool(Field& field, void* fieldParent)	{ return _serialize<bool>(writer, field, fieldParent); }
roStatus JsonOutputSerializer::serialize_int8(Field& field, void* fieldParent)	{ return _serialize<roInt8>(writer, field, fieldParent); }
roStatus JsonOutputSerializer::serialize_int16(Field& field, void* fieldParent)	{ return _serialize<roInt16>(writer, field, fieldParent); }
roStatus JsonOutputSerializer::serialize_int32(Field& field, void* fieldParent)	{ return _serialize<roInt32>(writer, field, fieldParent); }
roStatus JsonOutputSerializer::serialize_int64(Field& field, void* fieldParent)	{ return _serialize<roInt64>(writer, field, fieldParent); }
roStatus JsonOutputSerializer::serialize_uint8(Field& field, void* fieldParent)	{ return _serialize<roUint8>(writer, field, fieldParent); }
roStatus JsonOutputSerializer::serialize_uint16(Field& field, void* fieldParent){ return _serialize<roUint16>(writer, field, fieldParent); }
roStatus JsonOutputSerializer::serialize_uint32(Field& field, void* fieldParent){ return _serialize<roUint32>(writer, field, fieldParent); }
roStatus JsonOutputSerializer::serialize_uint64(Field& field, void* fieldParent){ return _serialize<roUint64>(writer, field, fieldParent); }
roStatus JsonOutputSerializer::serialize_float(Field& field, void* fieldParent)	{ return _serialize<float>(writer, field, fieldParent); }
roStatus JsonOutputSerializer::serialize_double(Field& field, void* fieldParent){ return _serialize<double>(writer, field, fieldParent); }

roStatus JsonOutputSerializer::serialize_string(Field& field, void* fieldParent)
{
	if(!fieldParent) return roStatus::pointer_is_null;
	const char** val = (const char**)(field.getConstPtr(fieldParent));
	return field.name.isEmpty() ? writer.write(*val) : writer.write(field.name.c_str(), *val);
}

roStatus JsonOutputSerializer::serialize_object(Field& field, void* fieldParent)
{
	Type* type = field.type;
	if(!type) return roStatus::pointer_is_null;

	const char* name = field.name.isEmpty() ? NULL : field.name.c_str();
	writer.beginObject(name);

	for(Field& f : type->fields) {
		if(!f.type) return roStatus::pointer_is_null;
		if(f.isConst) continue;	// We simply ignore any const member
		roStatus st = f.type->serializeFunc(*this, f, fieldParent);
		if(!st) return st;
	}

	writer.endObject();

	return roStatus::ok;
}

roStatus JsonOutputSerializer::beginArray(Field& field, roSize count)
{
	return writer.beginArray(field.name.isEmpty() ? NULL : field.name.c_str());
}

roStatus JsonOutputSerializer::endArray()
{
	return writer.endArray();
}

roStatus JsonOutputSerializer::serialize(float& val)
{
	return writer.write(val);
}

roStatus JsonOutputSerializer::serialize(double& val)
{
	return writer.write(val);
}

roStatus JsonOutputSerializer::serialize(const roUtf8*& val)
{
	return writer.write(val);
}

roStatus JsonOutputSerializer::serialize(roByte*& val, roSize& size)
{
	return writer.write(val, size);
}

roStatus JsonOutputSerializer::serialize(float* valArray, roSize count)
{
	for(roSize i=0; i<count; ++i) {
		roStatus st = writer.write(valArray[i]);
		if(!st) return st;
	}

	return roStatus::ok;
}

bool JsonOutputSerializer::isArrayEnded()
{
	roAssert(false);
	return true;
}


roStatus JsonInputSerializer::beginArchive()
{
	JsonParser::Event::Enum e = parser.nextEvent();
	parser.nextEvent();	// Make the parser one event ahead of the serialization system
	return e == JsonParser::Event::BeginObject ? roStatus::ok : roStatus::json_missing_root_object;
}

roStatus JsonInputSerializer::endArchive()
{
	JsonParser::Event::Enum e = parser.currentEvent();
	return e == JsonParser::Event::EndObject ? roStatus::ok : roStatus::json_missing_root_object;
}

roStatus JsonInputSerializer::serialize_bool(Field& field, void* fieldParent)
{
	if(!fieldParent) return roStatus::pointer_is_null;
	bool* val = reinterpret_cast<bool*>(field.getPtr(fieldParent));
	if(!val) return roStatus::pointer_is_null;

	roStatus st = _checkName(field); if(!st) return st;
	*val = parser.getBool();
	parser.nextEvent();
	return roStatus::ok;
}

template<class T>
roStatus _serialize(JsonInputSerializer& se, Field& field, void* fieldParent)
{
	if(!fieldParent) return roStatus::pointer_is_null;
	T* val = reinterpret_cast<T*>(field.getPtr(fieldParent));
	if(!val) return roStatus::pointer_is_null;

	roStatus st = se._checkName(field); if(!st) return st;
	st = se.parser.getNumber(*val); if(!st) return st;
	se.parser.nextEvent();
	return roStatus::ok;
}

roStatus JsonInputSerializer::serialize_int8(Field& field, void* fieldParent)	{ return _serialize<roInt8>(*this, field, fieldParent); }
roStatus JsonInputSerializer::serialize_int16(Field& field, void* fieldParent)	{ return _serialize<roInt16>(*this, field, fieldParent); }
roStatus JsonInputSerializer::serialize_int32(Field& field, void* fieldParent)	{ return _serialize<roInt32>(*this, field, fieldParent); }
roStatus JsonInputSerializer::serialize_int64(Field& field, void* fieldParent)	{ return _serialize<roInt64>(*this, field, fieldParent); }
roStatus JsonInputSerializer::serialize_uint8(Field& field, void* fieldParent)	{ return _serialize<roUint8>(*this, field, fieldParent); }
roStatus JsonInputSerializer::serialize_uint16(Field& field, void* fieldParent)	{ return _serialize<roUint16>(*this, field, fieldParent); }
roStatus JsonInputSerializer::serialize_uint32(Field& field, void* fieldParent)	{ return _serialize<roUint32>(*this, field, fieldParent); }
roStatus JsonInputSerializer::serialize_uint64(Field& field, void* fieldParent)	{ return _serialize<roUint64>(*this, field, fieldParent); }
roStatus JsonInputSerializer::serialize_float(Field& field, void* fieldParent)	{ return _serialize<float>(*this, field, fieldParent); }
roStatus JsonInputSerializer::serialize_double(Field& field, void* fieldParent)	{ return _serialize<double>(*this, field, fieldParent); }


roStatus JsonInputSerializer::serialize_string(Field& field, void* fieldParent)
{
	if(!fieldParent) return roStatus::pointer_is_null;
	const roUtf8** val = reinterpret_cast<const roUtf8**>(field.getPtr(fieldParent));
	if(!val) return roStatus::pointer_is_null;

	roStatus st = _checkName(field); if(!st) return st;
	*val = parser.getString();
	parser.nextEvent();
	return roStatus::ok;
}

roStatus JsonInputSerializer::serialize_object(Field& field, void* fieldParent)
{
	Type* type = field.type;
	if(!type) return roStatus::pointer_is_null;

	roStatus st = _checkName(field); if(!st) return st;

	if(parser.getCurrentProcessNext() != JsonParser::Event::BeginObject)
		return roStatus::json_expect_object_begin;

	for(Field& f : type->fields) {
		if(!f.type) return roStatus::pointer_is_null;
		if(f.isConst) continue;	// We simply ignore any const member
		st = f.type->serializeFunc(*this, f, fieldParent);
		if(!st) return st;
	}

	if(parser.currentEvent() != JsonParser::Event::EndObject)
		return roStatus::json_expect_object_end;

	parser.nextEvent();
	return roStatus::ok;
}

roStatus JsonInputSerializer::beginArray(Field& field, roSize count)
{
	roStatus st = _checkName(field); if(!st) return st;

	if(parser.getCurrentProcessNext() != JsonParser::Event::BeginArray)
		return roStatus::json_expect_array_begin;

	return roStatus::ok;
}

roStatus JsonInputSerializer::endArray()
{
	if(parser.getCurrentProcessNext() != JsonParser::Event::EndArray)
		return roStatus::json_expect_array_end;

	return roStatus::ok;
}

roStatus JsonInputSerializer::serialize(float& val)
{
	roStatus st = parser.getNumber(val);
	if(!st) return st;
	parser.nextEvent();
	return roStatus::ok;
}

roStatus JsonInputSerializer::serialize(double& val)
{
	roStatus st = parser.getNumber(val);
	if(!st) return st;
	parser.nextEvent();
	return roStatus::ok;
}

roStatus JsonInputSerializer::serialize(const roUtf8*& val)
{
	if(parser.currentEvent() != JsonParser::Event::String)
		return roStatus::json_expect_string;

	val = parser.getString();
	parser.nextEvent();
	return roStatus::ok;
}

roStatus JsonInputSerializer::serialize(roByte*& val, roSize& size)
{
	if(parser.currentEvent() != JsonParser::Event::String)
		return roStatus::json_expect_string;

	val = (roByte*)parser.getString();
	size = parser.getStringLen();

	return roStatus::ok;
}

roStatus JsonInputSerializer::serialize(float* valArray, roSize count)
{
	for(roSize i=0; i<count; ++i) {
		roStatus st = parser.getNumber(valArray[i]);
		if(!st) return st;
		parser.nextEvent();
	}

	return roStatus::ok;
}

bool JsonInputSerializer::isArrayEnded()
{
	return parser.currentEvent() == JsonParser::Event::EndArray;
}

roStatus JsonInputSerializer::_checkName(Field& field)
{
	const char* name = field.name.isEmpty() ? NULL : field.name.c_str();
	if(!name) return roStatus::ok;

	if(parser.currentEvent() != JsonParser::Event::Name)
		return roStatus::json_expect_object_name;

	if(roStrCmp(parser.getName(), name) != 0)
		return roStatus::serialization_member_mismatch;

	parser.nextEvent();
	return roStatus::ok;
}

}	// namespace Reflection
}   // namespace ro
