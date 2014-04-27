#include "pch.h"
#include "../../roar/base/roCoroutine.h"
#include "../../roar/math/roRandom.h"

using namespace ro;

struct CoroutineTest {};

namespace {

struct DymmyCoroutine : public Coroutine {
	virtual void run() override {}
};

}

TEST_FIXTURE(CoroutineTest, empty)
{
	{	CoroutineScheduler scheduler;
	}

	{	CoroutineScheduler scheduler;
		scheduler.init();
	}

	{	CoroutineScheduler scheduler;
		scheduler.init();
		scheduler.update();
	}

	DymmyCoroutine producer;
}

namespace {

struct Producer;
struct Consumer;

struct Producer : public Coroutine
{
	Producer() : ended(false), yieldValue(0) {}

	virtual void run() override;

	bool ended;
	int yieldValue;
	Consumer* consumer;
};

struct Consumer : public Coroutine
{
	virtual void run() override;

	Producer* producer;
	Array<int> result;
};

void Producer::run()
{
	suspend();
	const int numbers[] = { 1, 2, 3 };
	for(roSize i=0; i<roCountof(numbers); ++i) {
		yieldValue = numbers[i];
		switchToCoroutine(*consumer);
		switchToCoroutine(*this);
		resume();
	}
	ended = true;
	consumer->resume();
}

void Consumer::run()
{
	while(true) {
		switchToCoroutine(*producer);
		if(producer->ended)
			break;

		result.pushBack(producer->yieldValue);
	}
}

}	// namespace

TEST_FIXTURE(CoroutineTest, producerConsumer)
{
	CoroutineScheduler scheduler;
	scheduler.init();

	Producer producer;
	Consumer consumer;

	producer.debugName = "Producer";
	consumer.debugName = "Consumer";

	// Let producer and consumer know each others
	producer.consumer = &consumer;
	consumer.producer = &producer;

	scheduler.add(producer);
	scheduler.add(consumer);

	scheduler.update();

	CHECK_EQUAL(3, consumer.result.size());
	CHECK_EQUAL(1, consumer.result[0]);
	CHECK_EQUAL(2, consumer.result[1]);
	CHECK_EQUAL(3, consumer.result[2]);
}

namespace {

// Test if the run() function can handle construction and destruction of Coroutine
struct SpawnCoroutine : public Coroutine
{
	SpawnCoroutine() : counter(0)
	{
		debugName = "SpawnCoroutine";
		++maxInstanceCount;
		roVerify(initStack());
	}

	virtual void run() override
	{
		if(counter > 0) {
			SpawnCoroutine* newCoroutine = new SpawnCoroutine;
			--counter;
			newCoroutine->counter = counter;
			scheduler->add(*newCoroutine);
		}
		else
			scheduler->requestStop();

		{	SpawnCoroutine dummy;
			scheduler->add(dummy);
			// Can be destructed immediately given the coroutine haven't been run()
		}

		// Also test the sleep function and yield a few times
		coSleep(0.01f);
		yield();
		coSleep(0.01f);
		coSleep(0);	// sleep 0 is equivalent to yield()
		yield();

		delete this;
	}	// run()

	int counter;
	static roSize maxInstanceCount;
};

roSize SpawnCoroutine::maxInstanceCount = 0;

}

TEST_FIXTURE(CoroutineTest, spawnCoroutineInCoroutine)
{
	CoroutineScheduler scheduler;
	scheduler.init();

	SpawnCoroutine* spawner = new SpawnCoroutine;
	spawner->counter = 3;

	scheduler.add(*spawner);
	scheduler.update();

	CHECK_EQUAL(9, SpawnCoroutine::maxInstanceCount);
}

#include "../../roar/base/roSocket.h"

namespace {

struct SocketTestServer : public Coroutine
{
	virtual void run() override
	{{
		roUtf8 buf[128];
		roSize readSize = 0;
		int ret = s.receive(buf, sizeof(buf), readSize);

		if(ret > 0) {
			buf[ret] = 0;
			printf("id:%u, content: %s", id, buf);
		}

		s.close();
	}	delete this;
	}	// run()

	CoSocket s;
	roSize id;
};

static DefaultAllocator _allocator;

struct SocketTestServerAcceptor : public Coroutine
{
	virtual void run() override
	{{
		CoSocket s;
		SockAddr anyAddr(SockAddr::ipAny(), 80);

		roVerify(0 == s.create(BsdSocket::TCP));
		roVerify(0 == s.bind(anyAddr));
		roVerify(0 == s.listen());

		roSize spawnCount = 0;
		while(true) {
			AutoPtr<SocketTestServer> server(_allocator.newObj<SocketTestServer>());
			if(s.accept(server->s) == 0) {
				server->id = spawnCount++;
				scheduler->add(*server);
				server.unref();
			}
			else {
				spawnCount = spawnCount;
			}
		}
	}	delete this;
	}	// run()
};

struct SocketTestClient : public Coroutine
{
	virtual void run() override
	{
		CoSocket s;
		SockAddr addr(SockAddr::ipLoopBack(), 80);

		roVerify(0 == s.create(BsdSocket::TCP));
		int ret = s.connect(addr);
		if(ret != 0)
			ret = ret;

		const roUtf8* request = "GET / HTTP/1.1\r\n\r\n";
		s.send(request, roStrLen(request));

		s.close();
	}	// run()
};

}	// namespace

TEST_FIXTURE(CoroutineTest, socket)
{
	CoSocket::initApplication();

	CoroutineScheduler scheduler;
	scheduler.init();

	SocketTestServerAcceptor* server = new SocketTestServerAcceptor;
	scheduler.add(*server);

	Array<SocketTestClient> clients;
	clients.resize(1024);
	for(roSize i=0; i<clients.size(); ++i)
		scheduler.add(clients[i]);

	scheduler.update();
	scheduler.stop();
}
