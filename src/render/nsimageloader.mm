#include "pch.h"
#include "../render/texture.h"
#include "../platform.h"
#import <UIKit/UIKit.h>

using namespace Render;

// When the hardware cannot handle non power of two texture,
// which power of two to choose for:
// 1 means the next bigger pot, 2 means the next smaller pot
static const unsigned npotHandlingFactor = 2;

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
		, width(0), height(0), texWidth(0), texHeight(0)
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
	unsigned width, height, texWidth, texHeight;
	CGImageRef image;
	unsigned pixelDataSize;
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

// http://www.gotow.net/creative/wordpress/?p=7
static CGImageRef shrinkImageToPOT(CGImageRef image)
{
	if(!image) return NULL;

	unsigned width = CGImageGetWidth(image);
	unsigned height = CGImageGetHeight(image);

	width = Driver::nextPowerOfTwo(width) / npotHandlingFactor;
	height = Driver::nextPowerOfTwo(height) / npotHandlingFactor;

	CGContextRef ctx =	CGBitmapContextCreate(
		NULL,
		width, height,
		CGImageGetBitsPerComponent(image),			// bites per component
		CGImageGetBitsPerPixel(image) / 8 * width,	// bytes per row
		CGImageGetColorSpace(image),				// NOTE: the color space need not to manual release
		CGImageGetAlphaInfo(image)					// alpha info
	);

	if(!ctx) return NULL;

	CGContextDrawImage(ctx, CGRectMake(0, 0, width, height), image);
	CGImageRef ret = CGBitmapContextCreateImage(ctx);
	CGContextRelease(ctx);

	return ret;
}

void NSImageLoader::load(TaskPool* taskPool)
{
	texture->scratch = this;
	int tId = TaskPool::threadId();
	Rhinoca* rh = manager->rhinoca;

	CGDataProviderRef dataProvider;

	void* f = io_open(rh, texture->uri(), tId);
	if(!f) {
		print(rh, "NSImageLoader: Fail to open file '%s'\n", texture->uri().c_str());
		goto Abort;
	}

	pixelDataFormat = Driver::BGR;

	CGDataProviderSequentialCallbacks cb;
	cb.version = 0;
	cb.getBytes = dataProviderGetBytes;
	cb.skipForward = dataProviderSkipForwardBytes;
	cb.rewind = dataProviderRewind;
	cb.releaseInfo = dataProviderRelease;

	dataProvider = CGDataProviderCreateSequential(f, &cb);

	if(uriExtensionMatch(texture->uri(), ".jpg") || uriExtensionMatch(texture->uri(), ".jpeg"))
		image = CGImageCreateWithJPEGDataProvider(dataProvider, NULL, true, kCGRenderingIntentDefault);
	else if(uriExtensionMatch(texture->uri(), ".png"))
		image = CGImageCreateWithPNGDataProvider(dataProvider, NULL, true, kCGRenderingIntentDefault);

	CGDataProviderRelease(dataProvider);	// Shall close the file

	width = texWidth = CGImageGetWidth(image);
	height = texHeight = CGImageGetHeight(image);

	if(!Driver::getCapability("npot")) {
		texWidth = Driver::nextPowerOfTwo(width) / npotHandlingFactor;
		texHeight = Driver::nextPowerOfTwo(height) / npotHandlingFactor;

		if(texWidth != width || texHeight != height) {
			CGImageRef newImg = shrinkImageToPOT(image);
			CGImageRelease(image);
			image = newImg;
		}
	}

	pixelDataSize = CGImageGetBytesPerRow(image) * texHeight;

	return;

Abort:
	if(f) io_close(f, tId);
	texture->state = Resource::Aborted;
}

static void rgbaSetAlphaToOne(unsigned char* data, unsigned rowPixels, unsigned rowBytes, unsigned rows)
{
	for(unsigned i=0; i<rows; ++i) {
		unsigned char* p = data + i * rowBytes;
		for(unsigned j=0; j<rowPixels; ++j) {
			p[3] = 255;
			p += 4;
		}
	}
}

static void argbToRgba(unsigned char* data, unsigned rowPixels, unsigned rowBytes, unsigned rows)
{
	for(unsigned i=0; i<rows; ++i) {
		unsigned char* p = data + i * rowBytes;
		for(unsigned j=0; j<rowPixels; ++j) {
			unsigned tmp[4] = { p[0], p[1], p[2], p[3] };
			p[0] = tmp[1]; p[1] = tmp[2]; p[2] = tmp[3]; p[3] = tmp[0];
			p += 4;
		}
	}
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

	CFDataRef data = CGDataProviderCopyData(CGImageGetDataProvider(image));
	pixels = (unsigned char*)CFDataGetBytePtr(data);

	// Choose OpenGL format
	switch(bpp)
	{
	case 32:
		internal = format = Driver::RGBA;
		switch(info & kCGBitmapAlphaInfoMask) {
		case kCGImageAlphaPremultipliedFirst:
		case kCGImageAlphaFirst:
			argbToRgba(pixels, rowPixels, rowBytes, texHeight);
			break;
		case kCGImageAlphaNoneSkipFirst:
			argbToRgba(pixels, rowPixels, rowBytes, texHeight);
		case kCGImageAlphaNoneSkipLast:
			// If the driver support converting RGBA to RGB, then there is no need to call rgbaSetAlphaToOne()
			//internal = Driver::RGB;
			rgbaSetAlphaToOne(pixels, rowPixels, rowBytes, texHeight);
		case kCGImageAlphaPremultipliedLast:
		case kCGImageAlphaLast:
			break;
		default:
			ASSERT(false);
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

	if(texture->create(texWidth, texHeight, internal, (const char*)pixels, pixelDataSize, format))
		texture->state = Resource::Loaded;
	else
		texture->state = Resource::Aborted;

	texture->width = width;
	texture->height = height;

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
		texture->taskReady = taskPool->addFinalized(loaderTask, 0, 0, ~taskPool->mainThreadId());
		texture->taskLoaded = taskPool->addFinalized(loaderTask, 0, texture->taskReady, taskPool->mainThreadId());

		return true;
	}

	return false;
}

}	// namespace Loader
