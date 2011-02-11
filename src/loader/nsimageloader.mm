#include "pch.h"
#include "../render/texture.h"
#include "../platform.h"
#import <UIKit/UIKit.h>

using namespace Render;

namespace Loader {

Resource* createImage(const char* uri, ResourceManager* mgr)
{
	if(uriExtensionMatch(uri, ".jpg") ||
	   uriExtensionMatch(uri, ".jpeg") ||
	   uriExtensionMatch(uri, ".png"))
		return new Render::Texture(uri);
	return NULL;
}

class NSImageLoader : public Task
{
public:
	NSImageLoader(Texture* t, ResourceManager* mgr)
		: texture(t), manager(mgr)
		, width(0), height(0)
		, image(NULL)
		, pixelDataFormat(Driver::RGBA)
	{}

	~NSImageLoader() {
		if(image) CGImageRelease(image);
	}

	virtual void run(TaskPool* taskPool);

	void load(TaskPool* taskPool);
	void commit(TaskPool* taskPool);

	Texture* texture;
	ResourceManager* manager;
	rhuint width, height;
	CGImageRef image;
	rhuint pixelDataSize;
	Texture::Format pixelDataFormat;
};

void NSImageLoader::run(TaskPool* taskPool)
{
	if(!texture->scratch)
		load(taskPool);
	else
		commit(taskPool);
}

static size_t dataProviderGetBytes(void* f, void* buffer, size_t count)
{
	return io_read(f, buffer, count, 0);
}

static off_t dataProviderSkipForwardBytes(void* f, off_t count)
{
	ASSERT(false);
	return 0;
}

static void dataProviderRewind(void* f)
{
//	ASSERT(false);
}

static void dataProviderRelease(void* f)
{
	io_close(f, 0);
}

void NSImageLoader::load(TaskPool* taskPool)
{
	texture->scratch = this;
	int tId = TaskPool::threadId();
	Rhinoca* rh = manager->rhinoca;

	CGDataProviderRef dataProvider;

	void* f = io_open(rh, texture->uri(), tId);
	if(!f) goto Abort;

	pixelDataFormat = Driver::BGR;

	CGDataProviderSequentialCallbacks cb;
	cb.version = 0;
	cb.getBytes = dataProviderGetBytes;
	cb.skipForward = dataProviderSkipForwardBytes;
	cb.rewind = dataProviderRewind;
	cb.releaseInfo = dataProviderRelease;

	dataProvider = CGDataProviderCreateSequential(f, &cb);
	image = CGImageCreateWithPNGDataProvider(dataProvider, NULL, true, kCGRenderingIntentDefault);
	CGDataProviderRelease(dataProvider);	// Shall close the file
	
	width = CGImageGetWidth(image);
	height = CGImageGetHeight(image);
	pixelDataSize = CGImageGetBytesPerRow(image) * height;

	return;

Abort:
	if(f) io_close(f, tId);
	texture->state = Resource::Aborted;
}

void NSImageLoader::commit(TaskPool* taskPool)
{
	ASSERT(texture->scratch == this);
	texture->scratch = NULL;

	unsigned components;
	unsigned rowBytes, rowPixels;		// Image size padded by CGImage
	CGBitmapInfo info;					// CGImage component layout info

	info = CGImageGetBitmapInfo(image);	// CGImage may return pixels in RGBA, BGRA, or ARGB order
	CGColorSpaceModel colormodel;		// CGImage colormodel (RGB, CMYK, paletted, etc)
	Texture::Format internal, format;
	unsigned char *pixels, *temp = NULL;

	size_t bpp = CGImageGetBitsPerPixel(image);
	colormodel = CGColorSpaceGetModel(CGImageGetColorSpace(image));

	if(bpp < 8 || bpp > 32 || (colormodel != kCGColorSpaceModelMonochrome && colormodel != kCGColorSpaceModelRGB))
	{
		// This loader does not support all possible CGImage types, such as paletted images
		texture->state = Resource::Aborted;
		delete this;
		return;
	}

	components = bpp >> 3;
	rowBytes = CGImageGetBytesPerRow(image);	// CGImage may pad rows
	rowPixels = rowBytes / components;
	width = CGImageGetWidth(image);
	height = CGImageGetHeight(image);

	// Choose OpenGL format
	switch(bpp)
	{
	case 32:
		internal = Driver::RGBA;
		switch(info & kCGBitmapAlphaInfoMask) {
		case kCGImageAlphaPremultipliedFirst:
		case kCGImageAlphaFirst:
		case kCGImageAlphaNoneSkipFirst:
			format = Driver::BGRA;
			break;
		default:
			format = Driver::RGBA;
		}
		break;
	case 24:
		internal = format = Driver::RGB;
		break;
	case 16:
		internal = format = Driver::LUMINANCE_ALPHA;
		break;
	case 8:
		internal = format = Driver::LUMINANCE;
		break;
	default:
		ASSERT(false);
	}

	CFDataRef data = CGDataProviderCopyData(CGImageGetDataProvider(image));
	pixels = (unsigned char*)CFDataGetBytePtr(data);

	if(texture->create(width, height, (const char*)pixels, pixelDataSize, format))
		texture->state = Resource::Loaded;
	else
		texture->state = Resource::Aborted;

    free(temp);

	delete this;
}

bool loadImage(Resource* resource, ResourceManager* mgr)
{
	const char* uri = resource->uri();
	if(uriExtensionMatch(uri, ".jpg") ||
	   uriExtensionMatch(uri, ".jpeg") ||
	   uriExtensionMatch(uri, ".png"))
	{
		TaskPool* taskPool = mgr->taskPool;
		
		Texture* texture = dynamic_cast<Texture*>(resource);
		
		NSImageLoader* loaderTask = new NSImageLoader(texture, mgr);
		texture->taskReady = taskPool->addFinalized(loaderTask);
		texture->taskLoaded = taskPool->addFinalized(loaderTask, 0, texture->taskReady, taskPool->mainThreadId());
		
		return true;
	}

	return false;
}

}	// namespace Loader
