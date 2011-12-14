#include "pch.h"
#include "../render/texture.h"
#include "../common.h"
#include "../rhlog.h"
#include "../platform.h"
#include "../../roar/base/roFileSystem.h"

// http://www.spacesimulator.net/tut4_3dsloader.html
// http://www.gamedev.net/reference/articles/article1966.asp

using namespace ro;
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
		: stream(NULL), texture(t), manager(mgr)
		, width(0), height(0)
		, pixelData(NULL), pixelDataSize(0), pixelDataFormat(Driver::RGBA)
		, aborted(false), headerLoaded(false), readyToCommit(false), flipVertical(false)
	{}

	~BmpLoader()
	{
		if(stream) fileSystem.closeFile(stream);
		rhinoca_free(pixelData);
	}

	override void run(TaskPool* taskPool);

protected:
	void load(TaskPool* taskPool);
	void commit(TaskPool* taskPool);

	void loadHeader();
	void loadPixelData();

	void* stream;
	Texture* texture;
	ResourceManager* manager;
	rhuint width, height;
	char* pixelData;
	rhuint pixelDataSize;
	Texture::Format pixelDataFormat;

	bool aborted;
	bool headerLoaded;
	bool readyToCommit;
	bool flipVertical;
	BITMAPFILEHEADER fileHeader;
	BITMAPINFOHEADER infoHeader;
};

void BmpLoader::run(TaskPool* taskPool)
{
	if(!readyToCommit)
		load(taskPool);
	else
		commit(taskPool);
}

void BmpLoader::load(TaskPool* taskPool)
{
	if(headerLoaded)
		loadPixelData();
	else
		loadHeader();
}

void BmpLoader::commit(TaskPool* taskPool)
{
	if(!aborted && texture->create(width, height, Driver::ANY, pixelData, pixelDataSize, pixelDataFormat))
		texture->state = Resource::Loaded;
	else
		texture->state = Resource::Aborted;

	delete this;
}

void BmpLoader::loadHeader()
{
	if(texture->state == Resource::Aborted) goto Abort;
	if(!stream) stream = fileSystem.openFile(texture->uri());
	if(!stream) {
		rhLog("error", "BmpLoader: Fail to open file '%s'\n", texture->uri().c_str());
		goto Abort;
	}

	// Windows.h gives us these types to work with the Bitmap files
	roAssert(sizeof(BITMAPFILEHEADER) == 14);
	roAssert(sizeof(BITMAPINFOHEADER) == 40);
	memset(&fileHeader, 0, sizeof(fileHeader));

	// If data not ready, give up in this round and do it again in next schedule
	if(!fileSystem.readReady(stream, sizeof(fileHeader) + sizeof(infoHeader)))
		return reSchedule();

	// Read the file header
	fileSystem.read(stream, &fileHeader, sizeof(fileHeader));

	// Check against the magic 2 bytes.
	// The value of 'BM' in integer is 19778 (assuming little endian)
	if(fileHeader.bfType != 19778u) {
		rhLog("error", "BitmapLoader: Invalid bitmap header, operation aborted\n");
		goto Abort;
	}

	fileSystem.read(stream, &infoHeader, sizeof(infoHeader));
	width = infoHeader.biWidth;

	pixelDataFormat = Driver::BGR;

	if(infoHeader.biBitCount != 24) {
		rhLog("error", "BitmapLoader: Only 24-bit color is supported, operation aborted\n");
		goto Abort;
	}

	if(infoHeader.biCompression != 0) {
		rhLog("error", "BitmapLoader: Compressed bmp is not supported, operation aborted\n");
		goto Abort;
	}

	if(infoHeader.biHeight > 0) {
		height = infoHeader.biHeight;
		flipVertical = true;
	}
	else {
		height = -infoHeader.biHeight;
		flipVertical = false;
	}

	headerLoaded = true;
	loadPixelData();
	return;

Abort:
	aborted = true;
}

void BmpLoader::loadPixelData()
{
	if(aborted || !stream) goto Abort;

	// Memory usage for one row of image
	const rhuint rowByte = width * (sizeof(char) * 3);

	roAssert(!pixelData);
	pixelDataSize = rowByte * height;

	// If data not ready, give up in this round and do it again in next schedule
	if(!fileSystem.readReady(stream, pixelDataSize))
		return reSchedule();

	pixelData = (char*)rhinoca_malloc(pixelDataSize);

	if(!pixelData) {
		rhLog("error", "BitmapLoader: Corruption of file or not enough memory, operation aborted\n");
		goto Abort;
	}

	// At this point we can read every pixel of the image
	for(rhuint h = 0; h<height; ++h) {
		// Bitmap file is differ from other image format like jpg and png that
		// the vertical scan line order is inverted.
		const rhuint invertedH = flipVertical ? height - 1 - h : h;

		char* p = pixelData + (invertedH * rowByte);
		if(fileSystem.read(stream, p, rowByte) != rowByte) {
			rhLog("warn", "BitmapLoader: End of file, bitmap data incomplete\n");
			goto Abort;
		}
	}

	readyToCommit = true;
	return;

Abort:
	readyToCommit = true;
	aborted = true;
}

bool loadBmp(Resource* resource, ResourceManager* mgr)
{
	if(!uriExtensionMatch(resource->uri(), ".bmp")) return false;

	TaskPool* taskPool = mgr->taskPool;

	Texture* texture = dynamic_cast<Texture*>(resource);

	BmpLoader* loaderTask = new BmpLoader(texture, mgr);

	TaskId taskLoad = taskPool->addFinalized(loaderTask, 0, 0, ~taskPool->mainThreadId());
	texture->taskReady = texture->taskLoaded = taskPool->addFinalized(loaderTask, 0, taskLoad, taskPool->mainThreadId());

	return true;
}

}	// namespace Loader
