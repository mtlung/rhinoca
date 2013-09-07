#include "pch.h"
#include "../../src/common.h"
#include "../../roar/base/roLog.h"
#include "../../roar/base/roStopWatch.h"
#include "../../roar/base/roTaskPool.h"
#include "../../roar/base/roTypeOf.h"
#include "../../roar/base/roUtility.h"
#include "../../roar/platform/roPlatformHeaders.h"
#include "../../roar/math/roMath.h"
#include <math.h>
using namespace ro;

namespace {

class MyTask : public Task
{
public:
	MyTask(roSize multiplier) { countMultiplier = multiplier; }

	override void run(TaskPool* taskPool)
	{
		StopWatch watch;
		double mindt = +TypeOf<double>::valueMax();
		double maxdt = -TypeOf<double>::valueMax();
		double sumdt = 0;

		roUint64 tickBegin = ticksSinceProgramStatup();
		tickBegin = GetTickCount64();

		roSize count = 1000000 * countMultiplier;
		for(roSize i=count; i-- && taskPool->keepRun();)
		{
			double dt = watch.getAndReset();
			mindt = roMinOf2(dt, mindt);
			maxdt = roMaxOf2(dt, maxdt);
			sumdt += dt;

			for(roSize j=10; j--;)
				dt = sin(dt);
		}

		roUint64 tickEnd = ticksSinceProgramStatup();
		tickEnd = GetTickCount64();
		double diff = ticksToSeconds(tickEnd - tickBegin) - sumdt;
		diff = double(tickEnd - tickBegin) / 1000 - sumdt;
		double avgdt = sumdt / count;
		roLog("", "delta time min:%e, max:%e, avg:%e, diff%f\n", mindt, maxdt, avgdt, diff);

		delete this;
	}

	roSize countMultiplier;
};

class StopWatchTest {};

}	// namespace

TEST_FIXTURE(StopWatchTest, deviationFromGetTickCount)
{
	return;
	StopWatch watch;
	roUint64 t0 = GetTickCount64();

	roUint64 t1 = t0;
	while(true) {
		roUint64 t2 = GetTickCount64();
		if(t2 == t1)
			continue;

		double elapsed = watch.getDouble();
		double diff = elapsed - double(t2 - t1)/1000;
		roLog("", "diff: %f\n", diff);
	}
}

TEST_FIXTURE(StopWatchTest, threaded)
{
	return;
	TaskPool taskPool;
	taskPool.init(2);

	for(roSize i=10; i--;)
		taskPool.addFinalized(new MyTask(i), 0, 0, 0);

	taskPool.waitAll();
}
