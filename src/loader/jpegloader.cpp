#include "pch.h"
#include "../render/texture.h"
#include "../platform.h"

using namespace Render;

namespace Loader {

Resource* createJpeg(const char* uri, ResourceManager* mgr)
{
	if(uriExtensionMatch(uri, ".jpg") || uriExtensionMatch(uri, ".jpeg"))
		return new Render::Texture(uri);
	return NULL;
}

class JpegLoader : public Task
{
public:
	JpegLoader(Texture* t, ResourceManager* mgr)
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

void JpegLoader::run(TaskPool* taskPool)
{
	if(!texture->scratch)
		load(taskPool);
	else
		commit(taskPool);
}

void JpegLoader::load(TaskPool* taskPool)
{
	texture->scratch = this;
	int tId = TaskPool::threadId();
	Rhinoca* rh = manager->rhinoca;

	void* f = io_open(rh, texture->uri(), tId);
	if(!f) goto Abort;

	io_close(f, tId);
	return;

Abort:
	if(f) io_close(f, tId);
	texture->state = Resource::Aborted;
}

void JpegLoader::commit(TaskPool* taskPool)
{
	ASSERT(texture->scratch == this);
	texture->scratch = NULL;

	texture->width = width;
	texture->height = height;

	delete this;
}

bool loadJpeg(Resource* resource, ResourceManager* mgr)
{
	if(!uriExtensionMatch(resource->uri(), ".jpg") && !uriExtensionMatch(resource->uri(), ".jpeg")) return false;

	TaskPool* taskPool = mgr->taskPool;

	Texture* texture = dynamic_cast<Texture*>(resource);

	JpegLoader* loaderTask = new JpegLoader(texture, mgr);
	texture->taskReady = taskPool->addFinalized(loaderTask);
	texture->taskLoaded = taskPool->addFinalized(loaderTask, 0, texture->taskReady, taskPool->mainThreadId());

	return true;
}

}	// namespace Loader
