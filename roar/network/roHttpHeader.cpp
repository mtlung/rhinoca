#include "pch.h"
#include "roHttp.h"
#include "../base/roRegex.h"
#include "../base/roStringFormat.h"

namespace ro {

//////////////////////////////////////////////////////////////////////////
// HttpRequestHeader

Status HttpRequestHeader::makeGet(const char* resource)
{
	string.clear();
	return strFormat(string, "GET {} HTTP/1.1\r\n", resource);
}

Status HttpRequestHeader::addField(const char* option, const char* value)
{
	return strFormat(string, "{}: {}\r\n", option, value);
}

Status HttpRequestHeader::addField(HeaderField::Enum option, const char* value)
{
	switch(option) {
	case HeaderField::Accept:
		return addField("Accept", value);
	case HeaderField::AcceptCharset:
		return addField("Accept-Charset", value);
	case HeaderField::AcceptEncoding:
		return addField("Accept-Encoding", value);
	case HeaderField::AcceptLanguage:
		return addField("Accept-Language", value);
	case HeaderField::Authorization:
		return addField("Authorization", value);
	case HeaderField::CacheControl:
		return addField("Cache-Control", value);
	case HeaderField::Connection:
		return addField("Connection", value);
	case HeaderField::Cookie:
		return addField("Cookie", value);
	case HeaderField::ContentMD5:
		return addField("Content-MD5", value);
	case HeaderField::ContentType:
		return addField("Content-Type", value);
	case HeaderField::Expect:
		return addField("Expect", value);
	case HeaderField::From:
		return addField("From", value);
	case HeaderField::Host:
		return addField("Host", value);
	case HeaderField::Origin:
		return addField("Origin", value);
	case HeaderField::Pragma:
		return addField("Pragma", value);
	case HeaderField::ProxyAuthorization:
		return addField("Proxy-Authorization", value);
	case HeaderField::Referer:
		return addField("Referer", value);
	case HeaderField::UserAgent:
		return addField("UserAgent", value);
	case HeaderField::Via:
		return addField("Via", value);
	}

	return Status::invalid_parameter;
}

Status HttpRequestHeader::addField(HeaderField::Enum option, roUint64 value)
{
	switch(option) {
	case HeaderField::ContentLength:
		return strFormat(string, "{}: {}\r\n", "Content-Length", value);
	case HeaderField::MaxForwards:
		return strFormat(string, "{}: {}\r\n", "Max-Forwards", value);
	case HeaderField::Range:
		return strFormat(string, "Range: bytes={}-\r\n", value);
	}

	return Status::invalid_parameter;
}

Status HttpRequestHeader::addField(HeaderField::Enum option, roUint64 value1, roUint64 value2)
{
	switch(option) {
	case HeaderField::Range:
		return strFormat(string, "Range: bytes={}-{}\r\n", value1, value2);
	}

	return Status::invalid_parameter;
}

static bool _getField(const String& string, const char* option, RangedString& value)
{
	char* begin = const_cast<char*>(string.c_str());

	while(begin < string.end())
	{
		// Tokenize the string by new line
		char* end = roStrStr(begin, string.end(), "\r\n");

		if(begin == end)
			break;

		// Split by ':'
		char* p = roStrStr(begin, end, ":");

		if(p && roStrStrCase(begin, p, option)) {
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

bool HttpRequestHeader::getField(const char* option, RangedString& value) const
{
	return _getField(string, option, value);
}

bool HttpRequestHeader::getField(HeaderField::Enum option, RangedString& value) const
{
	switch(option) {
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


//////////////////////////////////////////////////////////////////////////
// HttpResponseHeader

bool HttpResponseHeader::getField(const char* option, RangedString& value) const
{
	return _getField(string, option, value);
}

bool HttpResponseHeader::getField(HeaderField::Enum option, RangedString& value) const
{
	switch(option) {
	case HeaderField::AccessControlAllowOrigin:
		return getField("Access-Control-Allow-Origin", value);
	case HeaderField::AcceptRanges:
		return getField("Accept-Ranges", value);
	case HeaderField::Allow:
		return getField("Allow", value);
	case HeaderField::CacheControl:
		return getField("Cache-Control", value);
	case HeaderField::Connection:
		return getField("Connection", value);
	case HeaderField::ContentEncoding:
		return getField("Content-Encoding", value);
	case HeaderField::ContentLanguage:
		return getField("Content-Language", value);
	case HeaderField::ContentLocation:
		return getField("Content-Location", value);
	case HeaderField::ContentMD5:
		return getField("Content-MD5", value);
	case HeaderField::ContentDisposition:
		return getField("Content-Disposition", value);
	case HeaderField::ContentType:
		return getField("Content-Type", value);
	case HeaderField::ETag:
		return getField("ETag", value);
	case HeaderField::Link:
		return getField("Link", value);
	case HeaderField::Location:
		return getField("Location", value);
	case HeaderField::Pragma:
		return getField("Pragma", value);
	case HeaderField::ProxyAuthenticate:
		return getField("Proxy-Authenticate", value);
	case HeaderField::Refresh:
		return getField("Refresh", value);
	case HeaderField::RetryAfter:
		return getField("RetryAfter", value);
	case HeaderField::Server:
		return getField("Server", value);
	case HeaderField::SetCookie:
		return getField("Set-Cookie", value);
	case HeaderField::Status:
		return getField("Status", value);
	case HeaderField::StrictTransportSecurity:
		return getField("Strict-Transport-Security", value);
	case HeaderField::Trailer:
		return getField("Trailer", value);
	case HeaderField::TransferEncoding:
		return getField("TransferEncoding", value);
	case HeaderField::Vary:
		return getField("Vary", value);
	case HeaderField::Version:
		{	Regex regex;
			regex.match(string.c_str(), "^HTTP/([\\d\\.]+)", "i");
			return regex.getValue(1, value);
		}
	case HeaderField::Via:
		return getField("Via", value);
	case HeaderField::WWWAuthenticate:
		return getField("WWWAuthenticate", value);
	}

	return false;
}

bool HttpResponseHeader::getField(HeaderField::Enum option, roUint64& value) const
{
	Regex regex;
	bool ok = false;
	RangedString str;

	switch(option) {
	case HeaderField::Age:
		ok = getField("Age", str);
		break;
	case HeaderField::ContentLength:
		ok = getField("Content-Length", str);
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

bool HttpResponseHeader::getField(HeaderField::Enum option, roUint64& value1, roUint64& value2, roUint64& value3) const
{
	Regex regex;
	RangedString str;

	switch(option) {
	case HeaderField::ContentRange:
		getField("Content-Range", str);
		regex.match(str, "^\\s*bytes\\s+([\\d]+)\\s*-\\s*([\\d]+)\\s*/([\\d]+)");
		if(!regex.getValue(1, value1)) return false;
		if(!regex.getValue(2, value2)) return false;
		return regex.getValue(3, value3);
	}

	return false;
}

}	// namespace ro
