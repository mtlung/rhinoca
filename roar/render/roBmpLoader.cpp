#include "pch.h"
#include "roTexture.h"
#include "roRenderDriver.h"
#include "../base/roFileSystem.h"
#include "../base/roLog.h"
#include "../base/roMemory.h"
#include "../platform/roPlatformHeaders.h"

// http://www.spacesimulator.net/tut4_3dsloader.html
// http://www.gamedev.net/reference/articles/article1966.asp

namespace ro {

Resource* resourceCreateBmp(const char* uri, ResourceManager* mgr)
{
	if(uriExtensionMatch(uri, ".bmp"))
		return new Texture(uri);
	return NULL;
}

class BmpLoader : public Task
{
public:
	BmpLoader(Texture* t, ResourceManager* mgr)
		: stream(NULL), texture(t), manager(mgr)
		, width(0), height(0)
		, pixelData(NULL), pixelDataSize(0)
		, aborted(false), headerLoaded(false), readyToCommit(false), flipVertical(false)
	{}

	~BmpLoader()
	{
		if(stream) fileSystem.closeFile(stream);
		roFree(pixelData);
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
	unsigned width, height;
	roBytePtr pixelData;
	roSize pixelDataSize;

	bool aborted;
	bool headerLoaded;
	bool readyToCommit;
	bool flipVertical;
	BITMAPFILEHEADER fileHeader;
	BITMAPINFOHEADER infoHeader;
};

void BmpLoader::run(TaskPool* taskPool)
{
	if(!readyToCommit && !aborted)
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
	if(	aborted ||
		!roRDriverCurrentContext->driver->initTexture(texture->handle, width, height, roRDriverTextureFormat_RGBA) ||
		!roRDriverCurrentContext->driver->commitTexture(texture->handle, pixelData, 0)
	)
		texture->state = Resource::Aborted;
	else
		texture->state = Resource::Loaded;

	delete this;
}

void BmpLoader::loadHeader()
{
	if(texture->state == Resource::Aborted) goto Abort;
	if(!stream) stream = fileSystem.openFile(texture->uri());
	if(!stream) {
		roLog("error", "BmpLoader: Fail to open file '%s'\n", texture->uri().c_str());
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
		roLog("error", "BitmapLoader: Invalid bitmap header, operation aborted\n");
		goto Abort;
	}

	fileSystem.read(stream, &infoHeader, sizeof(infoHeader));
	width = infoHeader.biWidth;

	if(infoHeader.biBitCount != 24) {
		roLog("error", "BitmapLoader: Only 24-bit color is supported, operation aborted\n");
		goto Abort;
	}

	if(infoHeader.biCompression != 0) {
		roLog("error", "BitmapLoader: Compressed bmp is not supported, operation aborted\n");
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

	flipVertical = false;	// TODO: Fix me

	headerLoaded = true;
	loadPixelData();
	return;

Abort:
	aborted = true;
}

static void _bgrToRgba(roBytePtr src, roBytePtr dst, unsigned width, unsigned height)
{
	for(unsigned h=0; h<height; ++h) for(unsigned w=0; w<width; ++w)
	{
		dst[0] = src[2];
		dst[1] = src[1];
		dst[2] = src[0];
		dst[3] = roUint8(-1);

		src += 3;
		dst += 4;
	}
}

void BmpLoader::loadPixelData()
{
	roBytePtr tmpBuffer = NULL;

	if(aborted || !stream) goto Abort;

	// Memory usage for one row of image
	const roSize rowByte = width * (sizeof(char) * 3);
	const roSize rowPadding = 4 - ((rowByte + 4) % 4);

	roAssert(!pixelData);
	pixelDataSize = rowByte * height;

	// If data not ready, give up in this round and do it again in next schedule
	if(!fileSystem.readReady(stream, pixelDataSize))
		return reSchedule();

	pixelData = roMalloc(pixelDataSize);

	if(!pixelData) {
		roLog("error", "BitmapLoader: Corruption of file or not enough memory, operation aborted\n");
		goto Abort;
	}

	char paddingBuf[4];

	// At this point we can read every pixel of the image
	for(unsigned h = 0; h<height; ++h) {
		// Bitmap file is differ from other image format like jpg and png that
		// the vertical scan line order is inverted.
		const unsigned invertedH = flipVertical ? height - 1 - h : h;

		roBytePtr p = pixelData + (invertedH * rowByte);

		if(fileSystem.read(stream, p, rowByte) != rowByte) {
			roLog("warn", "BitmapLoader: End of file, bitmap data incomplete\n");
			goto Abort;
		}

		roVerify(fileSystem.read(stream, paddingBuf, rowPadding) == rowPadding);
	}

	// Convert BGR to RGBA
	tmpBuffer = roMalloc(width * height * 4);
	_bgrToRgba(pixelData, tmpBuffer, width, height);
	roSwap(pixelData, tmpBuffer);
	roFree(tmpBuffer);

	readyToCommit = true;
	return;

Abort:
	readyToCommit = true;
	aborted = true;
}

bool resourceLoadBmp(Resource* resource, ResourceManager* mgr)
{
	if(!uriExtensionMatch(resource->uri(), ".bmp")) return false;

	TaskPool* taskPool = mgr->taskPool;

	Texture* texture = dynamic_cast<Texture*>(resource);
	texture->handle = roRDriverCurrentContext->driver->newTexture();

	BmpLoader* loaderTask = new BmpLoader(texture, mgr);

	TaskId taskLoad = taskPool->addFinalized(loaderTask, 0, 0, ~taskPool->mainThreadId());
	texture->taskReady = texture->taskLoaded = taskPool->addFinalized(loaderTask, 0, taskLoad, taskPool->mainThreadId());

	return true;
}

}	// namespace ro
