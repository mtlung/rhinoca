#include "pch.h"
#include "../../roar/base/roSocket.h"

using namespace ro;

namespace {

class SockAddrTestFixture
{
protected:
	SockAddrTestFixture()
	{
		roVerify(BsdSocket::initApplication() == 0);
	}

	~SockAddrTestFixture()
	{
		roVerify(BsdSocket::closeApplication() == 0);
	}
};	// SockAddrTestFixture

}	// namespace

TEST_FIXTURE(SockAddrTestFixture, Construct)
{
	// Construct SockAddr fron (IP) and an integer (port)
	SockAddr ep1(0u, 0u);
	SockAddr ep2(SockAddr::ipLoopBack(), 0xffffu);

	// Construct from a string
	CHECK(!ep1.parse(""));
	CHECK(ep1.parse("0.0.0.0:0"));
	CHECK(ep1.parse("127.0.0.1:80"));
	CHECK(ep1.parse("localhost:80"));
	CHECK(ep1.parse("yahoo.com:45678"));

	// Invalid address
//	CHECK(!ep1.parse("253.254.255.256:80"));

	// Invalid port
	CHECK(!ep1.parse("localhost:port1234"));
}

TEST_FIXTURE(SockAddrTestFixture, Getter)
{
	SockAddr ep1(0u, 0u);
	ep1.parse("127.0.0.1:80");
	CHECK(ep1.ip() == SockAddr::ipLoopBack());
	CHECK(ep1.port() == 80);

	//SockAddr ep2("127.001:65535");
	//CHECK(ep2.GetAddress() == SockAddr("127.0.0.1"));
	//CHECK(ep2.GetPort() == 65535);

	CHECK(ep1.parse("google.com:80"));
//	CHECK_EQUAL("google.com:80", ep1.getString());
//	CHECK_EQUAL("google.com", ep1.address().getString());
	CHECK_EQUAL(80, ep1.port());
}

TEST_FIXTURE(SockAddrTestFixture, Comparison)
{
	SockAddr ep1;
	CHECK(ep1.parse("127.0.0.1:80"));

	SockAddr ep2;
	CHECK(ep2.parse("localhost:80"));

	SockAddr ep3(0u, 0u);

	CHECK(ep1 == ep2);
	CHECK(ep2 != ep3);
}
