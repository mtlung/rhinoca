#include "pch.h"
#include "roHttp.h"
#include "../base/roRegex.h"
#include "../base/roStringFormat.h"

namespace ro {

//////////////////////////////////////////////////////////////////////////
// HttpRequestHeader

static const StaticArray<char*, 25>  _requestEnumStringMapping = {
	"Accept",
	"Accept-Charset",
	"Accept-Encoding",
	"Accept-Language",
	"Accept-Datetime",
	"Authorization",
	"Cache-Control",
	"Connection",
	"Cookie",
	"Content-Length",
	"Content-MD5",
	"Content-Type",
	"Date",
	"Expect",
	"From",
	"Host",
	"Max-Forwards",
	"Origin",
	"Resource",
	"Pragma",
	"Proxy-Authorization",
	"Range",
	"Referer",
	"User-Agent",
	"Via",
};

static const StaticArray<char*, 9>  _methodEnumStringMapping = {
	"GET",
	"HEAD" ,
	"POST",
	"PUT",
	"DELETE",
	"CONNECT",
	"OPTIONS",
	"TRACE",
	"INVALID",
};

Status HttpRequestHeader::make(Method::Enum method, const char* fullUrl)
{
	RangedString protocol, host, path;

	Status st = splitUrl(RangedString(fullUrl), protocol, host, path);
	if(!st) return st;

	string.clear();
	st = strFormat(string, "{} {}{} HTTP/1.1\r\n", _methodEnumStringMapping[method], fullUrl, path.size() ? "" : "/");
	if(!st) return st;

	return addField(HeaderField::Host, String(host).c_str());
}

Status HttpRequestHeader::addField(const char* field, const char* value)
{
	return strFormat(string, "{}: {}\r\n", field, value);
}

Status HttpRequestHeader::addField(HeaderField::Enum field, const char* value)
{
	return addField(_requestEnumStringMapping[field], value);
}

Status HttpRequestHeader::addField(HeaderField::Enum field, roUint64 value)
{
	switch(field) {
	case HeaderField::ContentLength:
		return strFormat(string, "{}: {}\r\n", "Content-Length", value);
	case HeaderField::MaxForwards:
		return strFormat(string, "{}: {}\r\n", "Max-Forwards", value);
	case HeaderField::Range:
		return strFormat(string, "Range: bytes={}-\r\n", value);
	}

	return Status::invalid_parameter;
}

Status HttpRequestHeader::addField(HeaderField::Enum field, roUint64 value1, roUint64 value2)
{
	switch(field) {
	case HeaderField::Range:
		return strFormat(string, "Range: bytes={}-{}\r\n", value1, value2);
	}

	return Status::invalid_parameter;
}

void HttpRequestHeader::removeField(const char* field)
{
	char* begin = const_cast<char*>(string.c_str());

	while(begin < string.end())
	{
		// Tokenize the string by new line
		char* end = roStrStr(begin, string.end(), "\r\n");

		if(begin == end)
			break;

		char* p = roStrStrCase(begin, end, field);
		if(p) {
			string.erase(p - string.c_str(), end - p + 2);
			return;
		}

		begin = end + 2;
	}
}

void HttpRequestHeader::removeField(HeaderField::Enum field)
{
	removeField(_requestEnumStringMapping[field]);
}

HttpRequestHeader::Method::Enum HttpRequestHeader::getMethod() const
{
	for(roSize i=0; i<_methodEnumStringMapping.size(); ++i) {
		char* method = roStrStrCase(const_cast<char*>(string.c_str()), _methodEnumStringMapping[i]);
		if(method && method == string.c_str())
			return static_cast<Method::Enum>(i);
	}

	return Method::Invalid;
}

static bool _getField(const String& string, const char* field, RangedString& value)
{
	const char* begin = const_cast<char*>(string.c_str());

	while(begin < string.end())
	{
		// Tokenize the string by new line
		const char* end = roStrStr(begin, string.end(), "\r\n");
		end = end ? end : string.end();

		if(begin == end)
			break;

		// Split by ':'
		const char* p = roStrStr(begin, end, ":");

		if(p && roStrStrCase(begin, p, field)) {
			++p;

			// Skip space
			while(*p == ' ') ++p;

			value = RangedString(p, end);
			return true;
		}

		begin = end + 2;
	}

	return false;
}

bool HttpRequestHeader::getField(const char* field, String& value) const
{
	RangedString rangedStr;
	if(!getField(field, rangedStr))
		return false;

	value = rangedStr;
	return true;
}

bool HttpRequestHeader::getField(const char* field, RangedString& value) const
{
	return _getField(string, field, value);
}

bool HttpRequestHeader::getField(HeaderField::Enum field, String& value) const
{
	RangedString rangedStr;
	if(!getField(field, rangedStr))
		return false;

	value = rangedStr;
	return true;
}

bool HttpRequestHeader::getField(HeaderField::Enum field, RangedString& value) const
{
	switch(field) {
	case HeaderField::Host:
		return getField("Host", value);
	case HeaderField::Resource:
		{	Regex regex;
			regex.match(string.c_str(), "^(GET|POST)\\s+([^\\s]*)", "i");
			return regex.getValue(2, value);
		}
	}

	return false;
}

bool HttpRequestHeader::cmpFieldNoCase(HeaderField::Enum option, const char* value) const
{
	RangedString str;
	if(!getField(option, str))
		return false;

	return str.cmpNoCase(value) == 0;
}


//////////////////////////////////////////////////////////////////////////
// HttpResponseHeader

static const StaticArray<char*, 35>  _responseEnumStringMapping = {
	"Access-Control-Allow-Origin",
	"Accept-Ranges",
	"Age",
	"Allow",
	"Cache-Control",
	"Connection",
	"Content-Encoding",
	"Content-Language",
	"Content-Length",
	"Content-Location",
	"Content-MD5",
	"Content-Disposition",
	"Content-Range",
	"Content-Type",
	"Date",
	"ETag",
	"Expires",
	"Last-Modified",
	"Link",
	"Location",
	"P3P",
	"Pragma",
	"Proxy-Authenticate",
	"Refresh",
	"Retry-After",
	"Server",
	"Set-Cookie",
	"Status",
	"Strict-Transport-Security",
	"Trailer",
	"Transfer-Encoding",
	"Vary",
	"Version",
	"Via",
	"WWW-Authenticate",
};

bool HttpResponseHeader::getField(const char* field, String& value) const
{
	RangedString rangedStr;
	if(!getField(field, rangedStr))
		return false;

	value = rangedStr;
	return true;
}

bool HttpResponseHeader::getField(const char* field, RangedString& value) const
{
	return _getField(string, field, value);
}

bool HttpResponseHeader::getField(HeaderField::Enum field, String& value) const
{
	RangedString rangedStr;
	if(!getField(field, rangedStr))
		return false;

	value = rangedStr;
	return true;
}

bool HttpResponseHeader::getField(HeaderField::Enum field, RangedString& value) const
{
	switch(field) {
	case HeaderField::Version:
		{	Regex regex;
			regex.match(string.c_str(), "^HTTP/([\\d\\.]+)", "i");
			return regex.getValue(1, value);
		}
	}

	return getField(_responseEnumStringMapping[field], value);
}

bool HttpResponseHeader::getField(HeaderField::Enum field, roUint64& value) const
{
	Regex regex;
	bool ok = false;
	RangedString str;

	switch(field) {
	case HeaderField::Age:
	case HeaderField::ContentLength:
		ok = getField(_responseEnumStringMapping[field], str);
		break;
	case HeaderField::Status:
		ok = regex.match(string.c_str(), "^HTTP/[\\d\\.]+[ ]+(\\d\\d\\d)", "i");
		if(regex.getValue(1, value))
			return true;

		ok = getField("Status", str);
		break;
	}

	if(!ok)
		return false;

	return roStrTo(str.begin, str.size(), value);
}

bool HttpResponseHeader::getField(HeaderField::Enum field, roUint64& value1, roUint64& value2, roUint64& value3) const
{
	Regex regex;
	RangedString str;

	switch(field) {
	case HeaderField::ContentRange:
		getField("Content-Range", str);
		regex.match(str, "^\\s*bytes\\s+([\\d]+)\\s*-\\s*([\\d]+)\\s*/([\\d]+)");
		if(!regex.getValue(1, value1)) return false;
		if(!regex.getValue(2, value2)) return false;
		return regex.getValue(3, value3);
	}

	return false;
}

bool HttpResponseHeader::cmpFieldNoCase(HeaderField::Enum option, const char* value) const
{
	RangedString str;
	if(!getField(option, str))
		return false;

	return str.cmpNoCase(value) == 0;
}

roStatus splitUrl(const RangedString& url, RangedString& protocol, RangedString& host, RangedString& path)
{
	Regex regex;
	if(!regex.match(url, "^\\s*([A-Za-z]+)://([^/]+)([^\\r\\n]*)"))
		return Status::http_invalid_uri;

	protocol = regex.result[1];
	host = regex.result[2];
	path = regex.result[3];

	return Status::ok;
}

}	// namespace ro
