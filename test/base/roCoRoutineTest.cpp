#include "pch.h"
#include "../../roar/base/roCoroutine.h"
#include "../../roar/math/roRandom.h"

using namespace ro;

struct CoroutineTest {};

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

	Producer producer;
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
