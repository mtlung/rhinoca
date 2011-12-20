#include "pch.h"
#include "../../roar/base/roSocket.h"
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
	BsdSocket::ErrorCode listenOn(BsdSocket& s)
	{
		if(s.create(BsdSocket::TCP) != 0) return s.lastError;
		if(s.bind(mAnyAddr) != 0) return s.lastError;
		return s.listen();
	}

	//!	Using non-blocking mode to setup a connection quickly
	BsdSocket::ErrorCode setupConnectedTcpSockets(BsdSocket& listener, BsdSocket& acceptor, BsdSocket& connector)
	{
		if(listenOn(listener) != 0) return listener.lastError;
		if(listener.setBlocking(false) != 0) return listener.lastError;

		if(!BsdSocket::inProgress(listener.accept(acceptor))) return listener.lastError;

		if(connector.create(BsdSocket::TCP) != 0) return listener.lastError;
		while(BsdSocket::inProgress(connector.connect(mLocalAddr))) {}
		if(connector.setBlocking(false) != 0) return listener.lastError;

		while(BsdSocket::inProgress(listener.accept(acceptor))) {}
		return acceptor.setBlocking(false);
	}

//	Thread mThread;
	SockAddr mLocalAddr;
	SockAddr mAnyAddr;
};	// BsdSocketTestFixture

struct SimpleConnector : public Task
{
	SimpleConnector(const SockAddr& ep) : endPoint(ep) {}

	override void run(TaskPool* taskPool) throw() {
		BsdSocket s;
		roVerify(0 == s.create(BsdSocket::TCP));
		bool connected = false;
		while(taskPool->keepRun()) {
			connected = s.connect(endPoint) == 0;

			if(connected) {
				const char msg[] = "Hello world!";
				roVerify(sizeof(msg) == s.send(msg, sizeof(msg)));
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
	a.parse("localhost:2");
	a.ip();
	BsdSocket s1;
	CHECK_EQUAL(0, listenOn(s1));

	SimpleConnector connector(mLocalAddr);
	TaskPool taskPool;
	taskPool.init(1);
	TaskId task = taskPool.addFinalized(&connector, 0, 0, ~taskPool.mainThreadId());

	BsdSocket s2;
	CHECK_EQUAL(0, s1.accept(s2));

	taskPool.wait(task);
}

TEST_FIXTURE(BsdSocketTestFixture, NonBlockingAccept)
{
	BsdSocket s1;
	CHECK_EQUAL(0, listenOn(s1));

	BsdSocket s2;
	CHECK_EQUAL(0, s1.setBlocking(false));
	CHECK(BsdSocket::inProgress(s1.accept(s2)));
	CHECK(BsdSocket::inProgress(s1.lastError));

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
	CHECK_EQUAL(0, listenOn(s1));

	SimpleConnector connector(mLocalAddr);
	TaskPool taskPool;
	taskPool.init(1);
	TaskId task = taskPool.addFinalized(&connector);

	BsdSocket s2;
	CHECK_EQUAL(0, s1.accept(s2));

	char buf[64];
	// TODO: Use loop for more robust code
	int receivedSize = s2.receive(buf, sizeof(buf));
	CHECK(receivedSize <= sizeof(buf));

	CHECK(strcmp("Hello world!", buf) == 0);
	taskPool.wait(task);
}

TEST_FIXTURE(BsdSocketTestFixture, Shutdown)
{
	BsdSocket sl, sa, sc;
	char buf[64];
	CHECK_EQUAL(0, setupConnectedTcpSockets(sl, sa, sc));

	CHECK_EQUAL(-1, sc.receive(buf, sizeof(buf)));

	// Server shutdown write, client should receive 0
	CHECK_EQUAL(0, sa.shutDownWrite());
	CHECK_EQUAL(-1, sa.send(buf, sizeof(buf)));

	int rec;
	while((rec = sc.receive(buf, sizeof(buf))) == -1) {}
	CHECK_EQUAL(0, rec);
	CHECK_EQUAL(0, sc.shutDownRead());

	// Client shutdown write, server should receive 0
	CHECK_EQUAL(0, sc.shutDownWrite());
	CHECK_EQUAL(-1, sc.send(buf, sizeof(buf)));

	while((rec = sa.receive(buf, sizeof(buf))) == -1) {}
	CHECK_EQUAL(0, rec);
	CHECK_EQUAL(0, sa.shutDownRead());
}

TEST_FIXTURE(BsdSocketTestFixture, Udp)
{
	BsdSocket s;
	CHECK_EQUAL(0, s.create(BsdSocket::UDP));
	CHECK_EQUAL(0, s.bind(mAnyAddr));

	BsdSocket s2;
	const SockAddr connectorAddr(SockAddr::ipLoopBack(), 4321);	// Explicit end point for verification test
	CHECK_EQUAL(0, s2.create(BsdSocket::UDP));
	CHECK_EQUAL(0, s2.bind(connectorAddr));
	const char msg[] = "Hello world!";
	CHECK_EQUAL(int(sizeof(msg)), s2.sendTo(msg, sizeof(msg), mLocalAddr));

	char buf[64];
	SockAddr srcAddr(SockAddr::ipAny(), 0);
	CHECK_EQUAL(int(sizeof(msg)), s.receiveFrom(buf, sizeof(buf), srcAddr));
	CHECK(srcAddr == connectorAddr);
}