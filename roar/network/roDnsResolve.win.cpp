#include "pch.h"
#include "roDnsResolve.h"
#include "roSocket.h"
#include "../base/roArray.h"
#include "../base/roCoroutine.h"
#include "../base/roStopWatch.h"
#include "../platform/roPlatformHeaders.h"
#include <winsock2.h>
#include <windns.h>	// DNS API
#pragma comment(lib, "dnsapi.lib")

// Reference:
// http://users.jyu.fi/~vesal/windows/vlwinapp/winlogin/sorsat/example.c

using namespace ro;

#define WM_HOSTRESOLVED (WM_USER + 2)

roStatus roGetHostByName(const roUtf8* hostname, roUint32& ipv4, float timeout)
{
	ipv4 = 0;
	roStatus st = roStatus::ok;

	// First use DNS API to query the system DNS cache
	// Reference: http://support.microsoft.com/kb/831226
	if(true)
	{	PDNS_RECORD pDnsRecord = NULL;
		DNS_STATUS status = DnsQuery_UTF8(
			hostname,
			DNS_TYPE_A,					// Type of the record to be queried.
			DNS_QUERY_NO_WIRE_QUERY,	// We want cached value only
			NULL,						// Contains DNS server IP address.
			&pDnsRecord,				// Resource record that contains the response.
			NULL);						// Reserved for future use.

		if(status == S_OK && pDnsRecord) {
			ipv4 = pDnsRecord->Data.A.IpAddress;
			ipv4 = ntohl(ipv4);
			DnsRecordListFree(pDnsRecord, DnsFreeRecordListDeep);
			return st;
		}
	}

	// If DnsQuery failed, use WSACancelAsyncRequest

	// The simplest way to create dummy window
	// http://stackoverflow.com/questions/5231186/simplest-way-to-create-a-hwnd
	HWND hWnd = ::CreateWindowA("STATIC", "roGetHostByName", WS_DISABLED, 0, 0, 100, 100, NULL, NULL, NULL, NULL);

	if(!hWnd)
		return roStatus::pointer_is_null;

	// NOTE: We cannot use a stack buffer here since the Coroutine will overwrite the
	// data written by WSAAsyncGetHostByName (which is running in another true thread)
	// after coSleep() finish
	ByteArray buf;
	buf.resizeNoInit(MAXGETHOSTSTRUCT);

	HANDLE hRet = WSAAsyncGetHostByName(
		hWnd, WM_HOSTRESOLVED,
		hostname,
		buf.castedPtr<char>(),
		(int)buf.sizeInByte()
	);

	if(!hRet)
		st = roStatus::pointer_is_null;

	CountDownTimer timer(timeout);

	for(bool keepRun=true; keepRun && st;)
	{
		MSG msg;

		if(timer.isExpired()) {
			if(WSACancelAsyncRequest(hRet) == S_OK)
				st = roStatus::timed_out;
		}

		while(::PeekMessage(&msg, hWnd, 0, 0, PM_REMOVE))
		{
			(void)TranslateMessage(&msg);
			(void)DispatchMessage(&msg);

			switch(msg.message)
			{
			case WM_HOSTRESOLVED:
				roAssert(hRet == (HANDLE)msg.wParam);

				if(WSAGETASYNCERROR(msg.lParam)) {
					st = roStatus::net_resolve_host_fail;
				}
				else {
					hostent* h = buf.castedPtr<hostent>();
					for(roSize i=0; h->h_addr_list[i]; ++i) {
						ipv4 = *reinterpret_cast<roUint32*>(h->h_addr_list[i]);
						ipv4 = ntohl(ipv4);
					}
				}

				keepRun = false;
				break;
			case WM_QUIT:
				st = roStatus::user_abort;
			default:
				break;
			}
		}

		if(keepRun && st)
			coSleep(1.f / 60);
	}

	roVerify(DestroyWindow(hWnd));

	return st;
}
