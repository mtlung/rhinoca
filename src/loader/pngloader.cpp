#include "pch.h"
#include "../render/texture.h"
#include "../platform.h"

using namespace Render;

namespace Loader {

Resource* createPng(const char* uri, ResourceManager* mgr)
{
	if(uriExtensionMatch(uri, ".png"))
		return new Render::Texture(uri);
	return NULL;
}

class PngLoader : public Task
{
public:
	PngLoader(Texture* t, ResourceManager* mgr)
		: texture(t), manager(mgr)
		, width(0), height(0)
	{}

	virtual void run(TaskPool* taskPool);

	void load(TaskPool* taskPool);
	void commit(TaskPool* taskPool);

	Texture* texture;
	ResourceManager* manager;
	rhuint width, height;
};

void PngLoader::run(TaskPool* taskPool)
{
	if(!texture->scratch)
		load(taskPool);
	else
		commit(taskPool);
}

void PngLoader::load(TaskPool* taskPool)
{
	texture->scratch = this;
	int tId = TaskPool::threadId();
	Rhinoca* rh = manager->rhinoca;

	void* f = io_open(rh, texture->uri, tId);
	if(!f) goto Abort;

	io_close(f, tId);
	return;

Abort:
	if(f) io_close(f, tId);
	texture->state = Resource::Aborted;
}

void PngLoader::commit(TaskPool* taskPool)
{
	ASSERT(texture->scratch == this);
	texture->scratch = NULL;

	texture->width = width;
	texture->height = height;

	delete this;
}

bool loadPng(Resource* resource, ResourceManager* mgr)
{
	if(!uriExtensionMatch(resource->uri, ".png")) return false;

	TaskPool* taskPool = mgr->taskPool;

	Texture* texture = dynamic_cast<Texture*>(resource);

	PngLoader* loaderTask = new PngLoader(texture, mgr);
	texture->taskReady = taskPool->addFinalized(loaderTask);
	texture->taskLoaded = taskPool->addFinalized(loaderTask, 0, texture->taskReady, taskPool->mainThreadId());

	return true;
}

}	// namespace Loader
