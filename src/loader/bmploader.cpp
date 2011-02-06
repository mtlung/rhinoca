#include "pch.h"
#include "../render/texture.h"
#include "../platform.h"

// http://www.spacesimulator.net/tut4_3dsloader.html
// http://www.gamedev.net/reference/articles/article1966.asp

using namespace Render;

namespace Loader {

Resource* createBmp(const char* uri, ResourceManager* mgr)
{
	if(uriExtensionMatch(uri, ".bmp"))
		return new Render::Texture(uri);
	return NULL;
}

class BmpLoader : public Task
{
public:
	BmpLoader(Texture* t, ResourceManager* mgr)
		: texture(t), manager(mgr)
		, width(0), height(0)
		, pixelData(NULL), pixelDataSize(0), pixelDataFormat(Driver::RGBA)
	{}

	~BmpLoader() {
		rhinoca_free(pixelData);
	}

	virtual void run(TaskPool* taskPool);

	void load(TaskPool* taskPool);
	void commit(TaskPool* taskPool);

	Texture* texture;
	ResourceManager* manager;
	rhuint width, height;
	char* pixelData;
	rhuint pixelDataSize;
	Texture::Format pixelDataFormat;
};

void BmpLoader::run(TaskPool* taskPool)
{
	if(!texture->scratch)
		load(taskPool);
	else
		commit(taskPool);
}

void BmpLoader::load(TaskPool* taskPool)
{
	texture->scratch = this;
	int tId = TaskPool::threadId();
	Rhinoca* rh = manager->rhinoca;

	void* f = io_open(rh, texture->uri(), tId);
	if(!f) goto Abort;

	// Windows.h gives us these types to work with the Bitmap files
	ASSERT(sizeof(BITMAPFILEHEADER) == 14);
	ASSERT(sizeof(BITMAPINFOHEADER) == 40);
	BITMAPFILEHEADER fileHeader;
	BITMAPINFOHEADER infoHeader;
	memset(&fileHeader, 0, sizeof(fileHeader));

	// Read the file header
	io_read(f, &fileHeader, sizeof(fileHeader), tId);

	// Check against the magic 2 bytes.
	// The value of 'BM' in integer is 19778 (assuming little endian)
	if(fileHeader.bfType != 19778u) {
		print(rh, "BitmapLoader: Invalid bitmap header, operation aborted");
		goto Abort;
	}

	io_read(f, &infoHeader, sizeof(infoHeader), tId);
	width = infoHeader.biWidth;

	pixelDataFormat = Driver::BGR;

	if(infoHeader.biBitCount != 24) {
		print(rh, "BitmapLoader: Only 24-bit color is supported, operation aborted");
		goto Abort;
	}

	if(infoHeader.biCompression != 0) {
		print(rh, "BitmapLoader: Compressed bmp is not supported, operation aborted");
		goto Abort;
	}

	bool flipVertical;
	if(infoHeader.biHeight > 0) {
		height = infoHeader.biHeight;
		flipVertical = true;
	}
	else {
		height = -infoHeader.biHeight;
		flipVertical = false;
	}

	// Memory usage for one row of image
	const rhuint rowByte = width * (sizeof(char) * 3);

	ASSERT(!pixelData);
	pixelDataSize = rowByte * height;
	pixelData = (char*)rhinoca_malloc(pixelDataSize);

	if(!pixelData) {
		print(rh, "BitmapLoader: Corruption of file or not enough memory, operation aborted");
		goto Abort;
	}

	// At this point we can read every pixel of the image
	for(rhuint h = 0; h<height; ++h) {
		// Bitmap file is differ from other image format like jpg and png that
		// the vertical scan line order is inverted.
		const rhuint invertedH = flipVertical ? height - 1 - h : h;

		char* p = pixelData + (invertedH * rowByte);
		if(io_read(f, p, rowByte, tId) != rowByte) {
			print(rh, "BitmapLoader: End of file, bitmap data incomplete");
			goto Abort;
		}
	}

	io_close(f, tId);
	return;

Abort:
	if(f) io_close(f, tId);
	texture->state = Resource::Aborted;
}

void BmpLoader::commit(TaskPool* taskPool)
{
	ASSERT(texture->scratch == this);
	texture->scratch = NULL;

	if(texture->create(width, height, pixelData, pixelDataSize, pixelDataFormat))
		texture->state = Resource::Loaded;
	else
		texture->state = Resource::Aborted;

	delete this;
}

bool loadBmp(Resource* resource, ResourceManager* mgr)
{
	if(!uriExtensionMatch(resource->uri(), ".bmp")) return false;

	TaskPool* taskPool = mgr->taskPool;

	Texture* texture = dynamic_cast<Texture*>(resource);

	BmpLoader* loaderTask = new BmpLoader(texture, mgr);
	texture->taskReady = taskPool->addFinalized(loaderTask);
	texture->taskLoaded = taskPool->addFinalized(loaderTask, 0, texture->taskReady, taskPool->mainThreadId());

	return true;
}

}	// namespace Loader
