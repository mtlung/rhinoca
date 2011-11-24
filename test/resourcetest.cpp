#include "pch.h"
#include "../src/resource.h"

using namespace ro;

namespace {

class MyResource : public Resource
{
public:
	MyResource(const char* uri) : Resource(uri), dummyData(0) {}
	int dummyData;
};

typedef IntrusivePtr<MyResource> MyResourcePtr;

Resource* create(const char* uri, ResourceManager* mgr)
{
	return new MyResource(uri);
}

bool load(Resource* resource, ResourceManager* mgr)
{
	class LoadTask : public Task
	{
	public:
		LoadTask(MyResource* r) : resource(r) {}
		override void run(TaskPool* taskPool)
		{
			resource->dummyData = 123;
			delete this;
		}

		MyResource* resource;
	};

	class CommitTask : public Task
	{
	public:
		CommitTask(MyResource* r) : resource(r) {}
		override void run(TaskPool* taskPool)
		{
			resource->state = Resource::Loaded;
			delete this;
		}

		MyResource* resource;
	};

	MyResource* r = dynamic_cast<MyResource*>(resource);
	TaskId loadId = mgr->taskPool->addFinalized(new LoadTask(r));
	TaskId commitId = mgr->taskPool->addFinalized(new CommitTask(r), 0, loadId, mgr->taskPool->mainThreadId());

	resource->taskReady = commitId;
	resource->taskLoaded = commitId;

	return true;
}

}	// namespace

TEST(ResourceTest)
{
	TaskPool taskPool;
	taskPool.init(1);
	ResourceManager mgr;
	mgr.taskPool = &taskPool;
	mgr.addFactory(create, load);

	MyResourcePtr r = mgr.loadAs<MyResource>("dummy");

	mgr.update();
	taskPool.doSomeTask();
}
