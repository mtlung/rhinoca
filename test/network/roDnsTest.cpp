#include "pch.h"
#include "../../roar/network/roDnsResolve.h"

using namespace ro;

struct DnsTest {};

TEST_FIXTURE(DnsTest, basic)
{
	roUint32 ip;
	CHECK_EQUAL(roStatus::ok, roGetHostByName("google.com", ip));
}
