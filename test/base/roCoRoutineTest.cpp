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
		CHECK_EQUAL(roStatus::already_initialized, scheduler.init());
		scheduler.stop();
	}

	{	CoroutineScheduler scheduler;
		CHECK_EQUAL(roStatus::already_initialized, scheduler.init());
		CHECK_EQUAL(roStatus::not_initialized, scheduler.update());
		scheduler.stop();
	}

	DymmyCoroutine dummy;
}

namespace {

struct Producer;
struct Consumer;

struct Producer : public Coroutine
{
	virtual void run() override;

	bool done = false;
	int yieldValue = 0;
	Consumer* consumer = nullptr;
};

struct Consumer : public Coroutine
{
	virtual void run() override;

	bool done = false;
	Producer* producer = nullptr;
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
	done = true;
	consumer->resume();
}

void Consumer::run()
{
	while(true) {
		switchToCoroutine(*producer);
		if(producer->done)
			break;

		result.pushBack(producer->yieldValue);
	}
	done = true;
}

}	// namespace

TEST_FIXTURE(CoroutineTest, producerConsumer)
{
	CoroutineScheduler& scheduler = *CoroutineScheduler::perThreadScheduler();

	Producer producer;
	Consumer consumer;

	producer.debugName = "Producer";
	consumer.debugName = "Consumer";

	// Let producer and consumer know each others
	producer.consumer = &consumer;
	consumer.producer = &producer;

	scheduler.add(producer);
	scheduler.add(consumer);

	while(!producer.done || !consumer.done)
		coYield();

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

}	// namespace

TEST_FIXTURE(CoroutineTest, spawnCoroutineInCoroutine)
{
	CoroutineScheduler& scheduler = *CoroutineScheduler::perThreadScheduler();

	SpawnCoroutine* spawner = new SpawnCoroutine;
	spawner->counter = 3;

	scheduler.add(*spawner);

	while(SpawnCoroutine::maxInstanceCount != 8)
		coYield();

	CHECK_EQUAL(8, SpawnCoroutine::maxInstanceCount);
}

#include "../../roar/network/roSocket.h"

TEST_FIXTURE(CoroutineTest, socket)
{
	CoSocket a;
	SockAddr anyAddr(SockAddr::ipAny(), 80);

	unsigned count = 1024;
	roVerify(a.create(BsdSocket::TCP));
	roVerify(a.bind(anyAddr));
	roVerify(a.listen(count));

	roSize inUseCount = 0;
	for (roSize i = 0; i < count; ++i) {
		CHECK(coRun([&]() {
			++inUseCount;

			CoSocket s;
			CHECK(a.accept(s));

			roUtf8 buf[128];
			roSize readSize = sizeof(buf);
			CHECK(s.receive(buf, readSize));
			CHECK(s.close());

			--inUseCount;
		}, "", roKB(8)));
	}

	for (roSize i = 0; i < count; ++i) {
		CoSocket s;
		SockAddr addr(SockAddr::ipLoopBack(), 80);

		CHECK(s.create(BsdSocket::TCP));
		CHECK(s.connect(addr, 100000));

		const roUtf8* request = "GET / HTTP/1.1\r\n\r\n";
		roSize len = roStrLen(request);
		CHECK(s.send(request, len));
	}

	while (inUseCount)
		coYield();
}
