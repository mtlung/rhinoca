#include "pch.h"
#include "roString.h"
#include "roStringUtility.h"
#include "roTypeCast.h"
#include "roUtility.h"
#include <malloc.h>
#include <string.h>

namespace ro {

static const char* _emptyString = "";

String::String()
	: _cstr(const_cast<char*>(_emptyString))
	, _length(0)
{
}

String::String(const char* str)
{
	_length = roStrLen(str);

	if(_length) {
		_cstr = (char*)malloc(_length + 1);
		roVerify(roStrnCpy(_cstr, str, _length) == _cstr);
		_cstr[_length] = '\0';
	}
	else
		_cstr = const_cast<char*>(_emptyString);
}

String::String(const char* str, roSize count)
{
	roAssert(count <= roStrLen(str));
	_length = count;

	if(_length) {
		_cstr = (char*)malloc(_length + 1);
		roVerify(roStrnCpy(_cstr, str, _length) == _cstr);
		_cstr[_length] = '\0';
	}
	else
		_cstr = const_cast<char*>(_emptyString);
}

String::String(const String& str)
{
	_length = str._length;

	if(_length) {
		_cstr = (char*)malloc(_length + 1);
		roVerify(roStrnCpy(_cstr, str._cstr, _length) == _cstr);
		_cstr[_length] = '\0';
	}
	else
		_cstr = const_cast<char*>(_emptyString);
}

String::~String()
{
	if(_length > 0)
		free(_cstr);
}

String& String::operator=(const char* str)
{
	String tmp(str);
	this->swap(tmp);
	return *this;
}

String& String::operator=(const String& str)
{
	String tmp(str);
	this->swap(tmp);
	return *this;
}

void String::resize(roSize size)
{
	if(size == 0)
		clear();
	else {
		_cstr = (char*)realloc(_length ? _cstr : NULL, size + 1);
		_length = size;
		_cstr[_length] = '\0';
	}
}

String& String::append(const char* str, roSize count)
{
	if(count > 0) {
		if(str[count - 1] == '\0') --count;	// Don't copy the null terminator, we will explicitly put one
		_cstr = (char*)realloc(_length ? _cstr : NULL, _length + count + 1);
		memcpy(_cstr + _length, str, count);
		_length += count;
		_cstr[_length] = '\0';
	}

	return *this;
}

String& String::assign(const char* str, roSize count)
{
	clear();
	return append(str, count);
}

void String::clear()
{
	erase(0);
}

String& String::erase(roSize offset, roSize count)
{
	if(_length == 0)
		return *this;

	roAssert(offset < _length);
	if(count > _length - offset)
		count = _length - offset;

	if(const roSize newLen = _length - count) {
		char* a = _cstr + offset;
		char* b = a + count;
		memmove(a, b, num_cast<roSize>(_cstr + _length - b));
		_cstr = (char*)realloc(_length ? _cstr : NULL, newLen + 1);
		_length = newLen;
		_cstr[_length] = '\0';
	}
	else
		clear();

	return *this;
}

roSize String::find(char c, roSize offset) const
{
	const char* ret = roStrChr(_cstr + offset, c);
	return ret ? ret - _cstr : npos;
}

roSize String::find(const char* str, roSize offset) const
{
	const char* ret = roStrStr(_cstr + offset, str);
	return ret ? ret - _cstr : npos;
}

roSize String::rfind(char c, roSize offset) const
{
	char str[] = { c, '\0' };
	return rfind(str, offset);
}

roSize String::rfind(const char* str, roSize offset) const
{
	roSize l1 = roMinOf2(offset, _length);
	roSize l2 = roStrLen(str);

	if(l2 > l1) return npos;

	for(char* s = _cstr + l1 - l2; s >= _cstr; --s)
		if(roStrnCmp(s, str, l2) == 0)
			return num_cast<roSize>(s - _cstr);
	return npos;
}

String String::substr(roSize offset, roSize count) const
{
	if(count == npos)
		return String(_cstr + offset);
	return String(_cstr + offset, count);
}

String& String::operator+=(const char* str)
{
	if(const roSize len = roStrLen(str)) {
		_cstr = (char*)realloc(_length ? _cstr : NULL, _length + len + 1);
		roVerify(strcat(_cstr, str) == _cstr);
		_length += len;
	}
	return *this;
}

String& String::operator+=(const String& str)
{
	if(!str.empty()) {
		_cstr = (char*)realloc(_length ? _cstr : NULL, _length + str._length + 1);
		roVerify(strcat(_cstr, str._cstr) == _cstr);
		_length += str._length;
	}
	return *this;
}

void String::swap(String& rhs)
{
	roSwap(_cstr, rhs._cstr);
	roSwap(_length, rhs._length);
}

}	// namespace
