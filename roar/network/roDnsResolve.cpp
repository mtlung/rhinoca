#include "pch.h"
#include "roDnsResolve.h"
#include "roSocket.h"
#include "../base/roArray.h"
#include "../base/roLog.h"
#include "../base/roStopWatch.h"
#include "../base/roString.h"
#include "../base/roTypeCast.h"
#include "../base/roMutex.h"
#include "../math/roRandom.h"
#include "../base/roUtility.h"

#if roOS_WIN				// Native Windows
#	include <winsock2.h>
#	include <svcguid.h>		// For using WSALookupService
#	include <windns.h>		// DNS API
#	pragma comment(lib, "dnsapi.lib")
#endif

// Reference:
// http://cr.yp.to/djbdns/notes.html
// http://const.me/articles/ms-dns-api/
// http://www.infologika.com.br/public/dnsquery_main.cpp
// http://www.binarytides.com/dns-query-code-in-c-with-winsock/
// http://www.tcpipguide.com/free/t_DNSMessageHeaderandQuestionSectionFormat.htm

// Other Async DNS library
// http://adns.sourceforge.net/

namespace ro {

namespace {

static const roSize _dnsUdpPacketSize = 512;

// Type field of Query and Answer
enum Type
{
	T_A		= 1,	// Host address
	T_NS	= 2,	// Authoritative server
	T_CNAME	= 5,	// Canonical name
	T_SOA	= 6,	// Start of authority zone
	T_PTR	= 12,	// domain name pointer
	T_MX	= 15	// mail routing information
};

// DNS header structure
struct DnsHeader
{
	roUint16	id;			// identification number

	roUint8		rd:1;		// recursion desired
	roUint8		tc:1;		// truncated message
	roUint8		aa:1;		// authoritative answer
	roUint8		opcode:4;	// purpose of message
	roUint8		qr:1;		// query/response flag

	roUint8		rcode:4;	// response code
	roUint8		cd:1;		// checking disabled
	roUint8		ad:1;		// authenticated data
	roUint8		z:1;		// its z! reserved
	roUint8		ra:1;		// recursion available

	roUint16	q_count;	// number of question entries
	roUint16	ans_count;	// number of answer entries
	roUint16	auth_count;	// number of authority entries
	roUint16	add_count;	// number of resource entries
};	// DnsHeader

// Constant sized fields of query structure
struct Question
{
	roUint16 qtype;
	roUint16 qclass;
};	// Question

// Constant sized fields of the resource record structure
#pragma pack(push, 1)
struct ResponseData
{
	roUint16 type;
	roUint16 _class;
	roUint32 ttl;
	roUint16 data_len;
};
#pragma pack(pop)

//Pointers to resource record contents
struct ResourceRecord
{
	String name;
	struct ResponseData* resource;
	String rdata;
};	// ResourceRecord

}	// namespace

// Eg. www.google.com -> 3www6google3com0
static roStatus _changetoDnsNameFormat(String& str)
{
	roStatus st;
	String out;
	st = str.append('.');
	if(!st) return st;

	for(roSize i=0; i<str.size(); ) {
		char* p = str.c_str() + i;
		char* next = roStrChr(p, '.');

		roUint8 num = 0;
		st = roSafeAssign(num, next - (str.c_str() + i));
		if(!st) return st;

		st = out.append(num, 1);
		if(!st) return st;
		st = out.append(p, num);
		if(!st) return st;

		i += num + 1;
	}

	st = out.append(char(0), 1);
	if(!st) return st;

	roSwap(out, str);
	return roStatus::ok;
}

static roStatus _fromDnsNameFormat(String& str)
{
	String out;

	// Convert 3www6google3com0 to www.google.com
	for(roSize i=0; i<str.size(); ++i) {
		roSize n = str[i];
		if(!str.isInRange(i+n))
			return roStatus::index_out_of_range;

		for(roSize j=0; j<n; ++j) {
			str[i] = str[i+1];
			++i;
		}

		if(i == str.size() - 1)
			str.resize(i);
		else
			str[i] = '.';
	}

	return roStatus::ok;
}


roStatus _readName(String& outStr, roByte* buffer, roByte*& reader, const roByte* readerEnd)
{
	if(reader >= readerEnd)
		return roStatus::index_out_of_range;

	outStr.clear();

	roSize jumped = 0, offset = 0, count = 1;
	roByte* orgReader = reader;

	while(*reader) {
		if(*reader >= 192) {
			offset = (*reader) * 256 + *(reader + 1) - 49152;	// 49152 = 11000000 00000000 ;)
			reader = buffer + offset - 1;
			jumped = 1;	// We have jumped to another location so counting wont go up!
		}
		else {
			roStatus st = outStr.append(*reader);
			if(!st) return st;
		}

		++reader;
		if(reader >= readerEnd)
			return roStatus::index_out_of_range;

		if(jumped == 0)
			++count; // If we haven't jumped to another location then we can count up
	}

	if(jumped == 1)
		++count;	//number of steps we actually moved forward in the packet

	reader = orgReader + count;

	return _fromDnsNameFormat(outStr);
}

struct DNSResolver
{
	DNSResolver() : isInited(false) {}

	roStatus init();

	SockAddr	getDNSAddr();
	bool		getCachedAddr(const char* host, SockAddr& addr);
	roStatus	resolve(const roUtf8* hostname, roUint32& ip, float timeout=5.f);

	bool isInited;
	roUint8 logLevel;
	RecursiveMutex mutex;
	TinyArray<SockAddr, 8> addresses;

#if roOS_WIN
	roStatus _initFunction();
	roStatus _getDnsServers();
	typedef struct _DNS_CACHE_ENTRY {
		struct _DNS_CACHE_ENTRY* pNext;	// Pointer to next entry
		PWSTR pszName;			// DNS Record Name
		roUint16 wType;			// DNS Record Type
		roUint16 wDataLength;	// Not referenced
		roUint32 dwFlags;		// DNS Record Flags
	} DNSCACHEENTRY, *PDNSCACHEENTRY;
	typedef int(WINAPI *DnsGetCacheDataTable)(PDNSCACHEENTRY*);
	DnsGetCacheDataTable _dnsGetCacheDataTable;
#endif
};	// DNSResolver

#if roOS_WIN
bool DNSResolver::getCachedAddr(const char* host, SockAddr& addr)
{
	if(!isInited && !init())
		return false;

#if 1
	// NOTE: Seems DNS_QUERY_NO_WIRE_QUERY didn't work correctly, it always fail
	return false;

	// Reference:
	// http://venom630.free.fr/geo/autre_chose/cachedns.html
	// http://stackoverflow.com/questions/7998176/retrieving-whats-in-the-dns-cache
	PDNSCACHEENTRY entry;
	if(!_dnsGetCacheDataTable || _dnsGetCacheDataTable(&entry) != TRUE)
		return false;

	Array<roUint16> wstr;
	{	roSize len = 0;
		if(!roUtf8ToUtf16(NULL, len, host, roSize(-1))) return false;
		if(!wstr.resize(len+1)) return false;
		if(!roUtf8ToUtf16(wstr.typedPtr(), len, host, roSize(-1))) return false;
		wstr[len] = 0;
	}

	for(; entry; entry = entry->pNext) {
		if(wcsicmp(entry->pszName, wstr.bytePtr()) != 0)
			continue;

		PDNS_RECORD pDnsRecord = NULL;
		DNS_STATUS status = DnsQuery_W(
			entry->pszName,
			DNS_TYPE_ANY,				// Type of the record to be queried.
			DNS_QUERY_NO_WIRE_QUERY,	// We want cached value only 
			NULL,						// Contains DNS server IP address.
			&pDnsRecord,				// Resource record that contains the response.
			NULL);						// Reserved for future use.

		if(status != S_OK || !pDnsRecord)
			return false;

		addr.setIp(ntohl(pDnsRecord->Data.A.IpAddress));
		DnsRecordListFree(pDnsRecord, DnsFreeRecordListDeep);
		break;
	}
#else
	// Reference: http://support.microsoft.com/kb/831226
	PDNS_RECORD pDnsRecord = NULL;
	DNS_STATUS status = DnsQuery_UTF8(
		host,
		DNS_TYPE_A,					// Type of the record to be queried.
		DNS_QUERY_NO_WIRE_QUERY,	// We want cached value only
		NULL,						// Contains DNS server IP address.
		&pDnsRecord,				// Resource record that contains the response.
		NULL);						// Reserved for future use.

	if(status != 0)
		return false;

	roUint32 addVal = pDnsRecord->Data.A.IpAddress;
	addVal = ntohl(addVal);
	addr.setIp(addVal);
	String str;
	addr.asString(str);
	DnsRecordListFree(pDnsRecord, DnsFreeRecordListDeep);
#endif

	if(logLevel > 0) {
		String str;
		addr.asString(str);
		roLog("info", "DNS cache found for %s, ip:%s\n", host, str.c_str());
	}

	return true;
}

roStatus DNSResolver::_initFunction()
{
	HINSTANCE hLib = ::LoadLibraryA("DNSAPI.dll");
	if(!hLib) return roStatus::pointer_is_null;

	_dnsGetCacheDataTable = (DnsGetCacheDataTable)GetProcAddress(hLib, "DnsGetCacheDataTable");
	if(!_dnsGetCacheDataTable) return roStatus::pointer_is_null;

	// Linking with dnsapi.lib means we need not to free hLib
//	::FreeLibrary(hLib);

	return roStatus::ok;
}

roStatus DNSResolver::_getDnsServers()
{
	roStatus st;
	GUID guid = SVCID_NAMESERVER_UDP;
	WSAQUERYSETA qs = { 0 };
	qs.dwSize = sizeof(WSAQUERYSETA);
	qs.dwNameSpace = NS_DNS;
	qs.lpServiceClassId = &guid;

	HANDLE hLookup;
	int ret = WSALookupServiceBeginA(&qs, LUP_RETURN_NAME|LUP_RETURN_ADDR, &hLookup);
	if(ret == SOCKET_ERROR) return st;

	// Loop through the services
	ByteArray buf;
	DWORD bufLen = num_cast<DWORD>(buf.size());

	while(true) {
		ret = WSALookupServiceNextA(hLookup, LUP_RETURN_NAME|LUP_RETURN_ADDR, &bufLen, (WSAQUERYSETA*)buf.bytePtr());
		if(ret == SOCKET_ERROR) {
			// Buffer too small?
			if(WSAGetLastError() == WSAEFAULT) {
				st = buf.resizeNoInit(bufLen);
				if(!st) return st;
				continue;
			}
			break;
		}

		WSAQUERYSET* pqs = (LPWSAQUERYSET)buf.bytePtr();
		CSADDR_INFO* pcsa = pqs->lpcsaBuffer;

		// Loop through the CSADDR_INFO array
		for(roSize i=0; i<pqs->dwNumberOfCsAddrs; ++i, ++pcsa) {
			SockAddr addr(*pcsa->RemoteAddr.lpSockaddr);
			addresses.pushBack(addr);
		}
	}

	return roStatus::ok;
}
#endif

roStatus DNSResolver::init()
{
	logLevel = 1;
	ScopeRecursiveLock lock(mutex);
	addresses.clear();

#if roOS_WIN
	roStatus st;
	st = _initFunction();
	if(!st) return st;

	st = _getDnsServers();
	if(!st) return st;
#endif

	{	// Use the open DNS servers as last resort
		SockAddr addr;
		if(addr.parse("208.67.222.220:53"))
			addresses.pushBack(addr);
		if(addr.parse("208.67.222.222:53"))
			addresses.pushBack(addr);
	}

	if(logLevel > 0) {
		roLog("info", "DNS server list:\n");
		for(roSize i=0; i<addresses.size(); ++i) {
			String str;
			addresses[i].asString(str);
			roLog("\t", "%s\n", str.c_str());
		}
	}

	isInited = true;
	return roStatus::ok;
}

SockAddr DNSResolver::getDNSAddr()
{
	ScopeRecursiveLock lock(mutex);

	if(!isInited)
		roVerify(init());

	// TODO: Instead of random, give higher priority to whom having less latency
	roSize index = roRandBeginEnd<roSize>(0, addresses.size());
	return addresses[index];
}

roStatus DNSResolver::resolve(const roUtf8* hostname_, roUint32& ip, float timeout)
{
	String hostname = hostname_;
	SockAddr addr;
	if(getCachedAddr(hostname.c_str(), addr))
		return roStatus::ok;

	roStatus st;

	CoSocket socket;
	st = socket.create(BsdSocket::UDP);
	if(!st) return st;

	ByteArray sendBuf;
	sendBuf.reserve(_dnsUdpPacketSize);

	ByteArray buf;
	buf.reserve(_dnsUdpPacketSize);

	const roUint16 randomKey = roRandBeginEnd<roUint16>(0, ro::TypeOf<roUint16>::valueMax());

	{	// Prepare request
		DnsHeader dnsHeader;
		roMemZeroStruct(dnsHeader);
		dnsHeader.id = htons(randomKey);
		dnsHeader.qr = 0;		// This is a query
		dnsHeader.opcode = 0;	// This is a standard query
		dnsHeader.aa = 0;		// Not Authoritative
		dnsHeader.tc = 0;		// This message is not truncated
		dnsHeader.rd = 1;		// Recursion Desired
		dnsHeader.ra = 0;		// Recursion not available
		dnsHeader.q_count = htons(1); //we have only 1 question

		st = _changetoDnsNameFormat(hostname);
		if(!st) return st;

		Question question;
		question.qtype = htons(1);	// We are requesting the ipv4 address
		question.qclass = htons(1);	// Its internet

		sendBuf.resize(sizeof(DnsHeader) + hostname.size() + sizeof(Question));
		roMemcpy(sendBuf.bytePtr(), &dnsHeader, sizeof(DnsHeader));
		roMemcpy(sendBuf.bytePtr() + sizeof(DnsHeader), hostname.c_str(), hostname.size());
		roMemcpy(sendBuf.bytePtr() + sizeof(DnsHeader) + hostname.size(), &question, sizeof(Question));
	}

	// Keep try sending request until timeout given something only minor failed
	CountDownTimer timer(timeout);
	DnsHeader* dnsHeader = NULL;
	roSize recvLen = 0;
	while(timeout == 0 || !timer.isExpired()) {
		SockAddr addr = getDNSAddr();

		if(logLevel > 0) {
			String str;
			addr.asString(str);
			roLog("info", "Sending request (%u) to DNS server %s to resolve %s\n", randomKey, str.c_str(), hostname_);
		}

		// Send request
		st = socket.sendTo(sendBuf.bytePtr(), sendBuf.sizeInByte(), addr);
		if(!st) return st;

		// Receive answer
		buf.resizeNoInit(_dnsUdpPacketSize);
		recvLen = buf.sizeInByte();
		st = socket.receiveFrom(buf.bytePtr(), recvLen, addr, 0.5f);

		if(st == roStatus::timed_out)
			continue;

		if(!st)
			return st;

		dnsHeader = buf.castedPtr<DnsHeader>();
		dnsHeader->id = ntohs(dnsHeader->id);
		dnsHeader->q_count = ntohs(dnsHeader->q_count);
		dnsHeader->ans_count = ntohs(dnsHeader->ans_count);
		dnsHeader->auth_count = ntohs(dnsHeader->auth_count);
		dnsHeader->add_count = ntohs(dnsHeader->add_count);

		if(dnsHeader->id != randomKey)
			continue;

		break;
	}

	if(timeout > 0 && timer.isExpired())
		return roStatus::timed_out;

	if(dnsHeader->qr != 1)
		return roStatus::data_corrupted;

	// TODO: Deal with truncated case, retry with TCP
//	if(dnsHeader->tc)	// If truncated
//		return roStatus::data_corrupted;

	if(dnsHeader->ans_count == 0)
		return roStatus::net_resolve_host_fail;

	Array<ResourceRecord> answers;

	st = answers.resize(dnsHeader->ans_count);
	if(!st) return st;

	roByte* reader = buf.bytePtr() + sizeof(DnsHeader) + hostname.size() + sizeof(Question);
	roByte* readerEnd = buf.bytePtr() + recvLen;

	if(logLevel > 0)
		roLog("info", "Reading DNS response (%u)\n", randomKey);

	// Reading answers
	// TODO: Use data stream to simplify the buffer bounds checking
	for(roSize i=0; i<dnsHeader->ans_count; ++i)
	{
		st = _readName(answers[i].name, buf.bytePtr(), reader, readerEnd);
		if(!st) return st;

		answers[i].resource = reinterpret_cast<ResponseData*>(reader);
		reader += sizeof(ResponseData);

		ResponseData& rdata = *answers[i].resource;
		rdata.type = ntohs(rdata.type);
		rdata._class = ntohs(rdata._class);
		rdata.ttl = ntohl(rdata.ttl);
		rdata.data_len = ntohs(rdata.data_len);

		struct sockaddr_in a;

		switch(rdata.type) {
		case T_A:	// If its an ipv4 address
			answers[i].rdata.resize(rdata.data_len);
			roMemcpy(answers[i].rdata.c_str(), reader, rdata.data_len);
			reader += rdata.data_len;

			a.sin_addr.s_addr = *(roUint32*)answers[i].rdata.c_str(); //working without ntohl

			if(logLevel > 0)
				roLog("\t", "%s %s\n", answers[i].name.c_str(), inet_ntoa(a.sin_addr));

			break;
		case T_MX:
		default:
			st = _readName(answers[i].rdata, buf.bytePtr(), reader, readerEnd);
			if(!st) return st;
			break;
		}
	}

	return roStatus::ok;
}

static DNSResolver _dnsResolver;

}	// namespace

roStatus roGetHostByName(const roUtf8* hostname, roUint32& ip, float timeout)
{
	return ro::_dnsResolver.resolve(hostname, ip, timeout);
}
