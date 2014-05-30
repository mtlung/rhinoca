#include "pch.h"
#include "../../roar/network/roDnsResolve.h"
#include "../../roar/network/roSocket.h"

using namespace ro;

struct DnsTest {};

TEST_FIXTURE(DnsTest, basic)
{
	roUint32 ipv4;
	roStatus st = roGetHostByName("google.com", ipv4, 1);
	CHECK_EQUAL(roStatus::ok, st);

	if(st) {
		SockAddr addr(ipv4, 0);
		String str;
		addr.asString(str);
		str = str;
	}
}
