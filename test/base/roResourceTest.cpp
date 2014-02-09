#include "pch.h"
#include "../../roar/base/roResource.h"

using namespace ro;

namespace {

class MyResource : public Resource
{
public:
	MyResource(const char* uri) : Resource(uri), dummyData(0) {}
	ConstString resourceType() const override { return "MyResource"; }
	int dummyData;
};

typedef SharedPtr<MyResource> MyResourcePtr;

Resource* create(ResourceManager* mgr, const char* uri)
{
	return new MyResource(uri);
}

bool load(ResourceManager* mgr, Resource* resource)
{
	class LoadTask : public Task
	{
	public:
		LoadTask(MyResource* r) : resource(r) {}
		void run(TaskPool* taskPool) override
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
		void run(TaskPool* taskPool) override
		{
			resource->state = Resource::Loaded;
			delete this;
		}

		MyResource* resource;
	};

	MyResource* r = dynamic_cast<MyResource*>(resource);
	TaskId loadId = mgr->taskPool->addFinalized(new LoadTask(r));
	TaskId commitId = mgr->taskPool->addFinalized(new CommitTask(r), 0, loadId, mgr->taskPool->mainThreadId());

	r->taskReady = commitId;
	r->taskLoaded = commitId;

	return true;
}

}	// namespace

TEST(ResourceTest)
{
	TaskPool taskPool;
	taskPool.init(1);
	ResourceManager mgr;
	mgr.taskPool = &taskPool;
	mgr.addLoader(create, load);

	MyResourcePtr r = mgr.loadAs<MyResource>(new MyResource(""), load);

	mgr.tick();
	taskPool.doSomeTask();
}
