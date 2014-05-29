#include "pch.h"
#include "../../roar/network/roSocket.h"
#include "../../roar/base/roTaskPool.h"

using namespace ro;

namespace {

class BsdSocketTestFixture
{
protected:
	BsdSocketTestFixture()
		: mLocalAddr(SockAddr::ipLoopBack(), 1234)
		, mAnyAddr(SockAddr::ipAny(), 1234)
	{
		roVerify(BsdSocket::initApplication() == 0);
	}

	~BsdSocketTestFixture()
	{
		roVerify(BsdSocket::closeApplication() == 0);
	}

	// Quickly create a listening socket
	roStatus listenOn(BsdSocket& s)
	{
		roStatus st;
		st = s.create(BsdSocket::TCP); if(!st) return st;
		st = s.bind(mAnyAddr); if(!st) return st;
		return s.listen();
	}

	//!	Using non-blocking mode to setup a connection quickly
	roStatus setupConnectedTcpSockets(BsdSocket& listener, BsdSocket& acceptor, BsdSocket& connector)
	{
		roStatus st = listenOn(listener); if(!st) return st;
		st = listener.setBlocking(false); if(!st) return st;

		st = listener.accept(acceptor);
		if(BsdSocket::isError(st)) return st;

		st = connector.create(BsdSocket::TCP); if(!st) return st;
		while(BsdSocket::inProgress(connector.connect(mLocalAddr))) {}
		st = connector.setBlocking(false); if(!st) return st;

		while(BsdSocket::inProgress(listener.accept(acceptor))) {}
		return acceptor.setBlocking(false); if(!st) return st;
	}

//	Thread mThread;
	SockAddr mLocalAddr;
	SockAddr mAnyAddr;
};	// BsdSocketTestFixture

struct SimpleConnector : public Task
{
	SimpleConnector(const SockAddr& ep) : endPoint(ep) {}

	void run(TaskPool* taskPool) override
	{
		BsdSocket s;
		roVerify(s.create(BsdSocket::TCP));
		roStatus st;
		while(taskPool->keepRun()) {
			if(s.connect(endPoint)) {
				const char msg[] = "Hello world!";
				roSize msgSize = sizeof(msg);
				roVerify(s.send(msg, msgSize));
				roVerify(msgSize == sizeof(msg));
				break;
			}
		}
	}

	SockAddr endPoint;
};	// SimpleConnector

}	// namespace

TEST_FIXTURE(BsdSocketTestFixture, BlockingAcceptAndConnect)
{
	SockAddr a;
	CHECK(a.parse("localhost:2"));
	a.ip();
	BsdSocket s1;
	CHECK(listenOn(s1));

	SimpleConnector connector(mLocalAddr);
	TaskPool taskPool;
	taskPool.init(1);
	TaskId task = taskPool.addFinalized(&connector, 0, 0, ~taskPool.mainThreadId());

	BsdSocket s2;
	CHECK(s1.accept(s2));

	taskPool.wait(task);
}

TEST_FIXTURE(BsdSocketTestFixture, NonBlockingAccept)
{
	BsdSocket s1;
	CHECK(listenOn(s1));

	BsdSocket s2;
	CHECK(s1.setBlocking(false));
	CHECK(BsdSocket::inProgress(s1.accept(s2)));

	SimpleConnector connector(mLocalAddr);
	TaskPool taskPool;
	taskPool.init(1);
	TaskId task = taskPool.addFinalized(&connector, 0, 0, ~taskPool.mainThreadId());

	while(BsdSocket::inProgress(s1.accept(s2))) {}

	taskPool.wait(task);
}

TEST_FIXTURE(BsdSocketTestFixture, TCPBlockingSendBlockingRecv)
{
	BsdSocket s1;
	CHECK(listenOn(s1));

	SimpleConnector connector(mLocalAddr);
	TaskPool taskPool;
	taskPool.init(1);
	TaskId task = taskPool.addFinalized(&connector);

	BsdSocket s2;
	CHECK(s1.accept(s2));

	char buf[64];
	// TODO: Use loop for more robust code
	roSize len = sizeof(buf);
	CHECK(s2.receive(buf, len));
	CHECK(len <= sizeof(buf));

	CHECK(roStrCmp("Hello world!", buf) == 0);
	taskPool.wait(task);
}

TEST_FIXTURE(BsdSocketTestFixture, Shutdown)
{
	BsdSocket sl, sa, sc;
	char buf[64];
	CHECK(setupConnectedTcpSockets(sl, sa, sc));

	roSize len = sizeof(buf);
	CHECK(!sc.receive(buf, len));

	// Server shutdown write, client should receive 0
	CHECK(sa.shutDownWrite());
	len = sizeof(buf);
	CHECK(!sa.send(buf, len));

	while(len = sizeof(buf), sc.receive(buf, len) && len) {}
	CHECK_EQUAL(0, len);
	CHECK(sc.shutDownRead());

	// Client shutdown write, server should receive 0
	CHECK(sc.shutDownWrite());
	len = sizeof(buf);
	CHECK(!sc.send(buf, len));

	while(len = sizeof(buf), sc.receive(buf, len) && len) {}
	CHECK_EQUAL(0, len);
	CHECK(sa.shutDownRead());
}

TEST_FIXTURE(BsdSocketTestFixture, Udp)
{
	BsdSocket s;
	CHECK(s.create(BsdSocket::UDP));
	CHECK(s.bind(mAnyAddr));

	BsdSocket s2;
	const SockAddr connectorAddr(SockAddr::ipLoopBack(), 4321);	// Explicit end point for verification test
	CHECK(s2.create(BsdSocket::UDP));
	CHECK(s2.bind(connectorAddr));
	const char msg[] = "Hello world!";
	CHECK(s2.sendTo(msg, sizeof(msg), mLocalAddr));

	char buf[64];
	SockAddr srcAddr(SockAddr::ipAny(), 0);
	roSize len = sizeof(buf);
	CHECK(s.receiveFrom(buf, len, srcAddr));
	CHECK(srcAddr == connectorAddr);
}
