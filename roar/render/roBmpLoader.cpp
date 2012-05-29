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

class BmpLoader : public Task
{
public:
	BmpLoader(Texture* t, ResourceManager* mgr)
		: stream(NULL), texture(t), manager(mgr)
		, width(0), height(0)
		, pixelData(NULL), pixelDataSize(0)
		, flipVertical(false)
		, nextFun(&BmpLoader::loadHeader)
	{}

	~BmpLoader()
	{
		if(stream) fileSystem.closeFile(stream);
	}

	override void run(TaskPool* taskPool);

protected:
	void loadHeader(TaskPool* taskPool);
	void initTexture(TaskPool* taskPool);
	void loadPixelData(TaskPool* taskPool);
	void commit(TaskPool* taskPool);
	void abort(TaskPool* taskPool);

	void* stream;
	Texture* texture;
	ResourceManager* manager;
	unsigned width, height;
	Array<roUint8> pixelData;
	roSize pixelDataSize;

	bool flipVertical;
	BITMAPFILEHEADER fileHeader;
	BITMAPINFOHEADER infoHeader;

	void (BmpLoader::*nextFun)(TaskPool*);
};

void BmpLoader::run(TaskPool* taskPool)
{
	if(texture->state == Resource::Aborted)
		nextFun = &BmpLoader::abort;

	(this->*nextFun)(taskPool);
}

void BmpLoader::loadHeader(TaskPool* taskPool)
{
	Status st;

roEXCP_TRY
	if(!stream) st = fileSystem.openFile(texture->uri(), stream);
	if(!st) roEXCP_THROW;

	// Windows.h gives us these types to work with the Bitmap files
	roAssert(sizeof(BITMAPFILEHEADER) == 14);
	roAssert(sizeof(BITMAPINFOHEADER) == 40);
	memset(&fileHeader, 0, sizeof(fileHeader));

	// If data not ready, give up in this round and do it again in next schedule
	if(fileSystem.readWillBlock(stream, sizeof(fileHeader) + sizeof(infoHeader)))
		return reSchedule();

	// Read the file header
	st = fileSystem.atomicRead(stream, &fileHeader, sizeof(fileHeader));
	if(!st) roEXCP_THROW;

	// Read the info header
	st = fileSystem.atomicRead(stream, &infoHeader, sizeof(infoHeader));
	if(!st) roEXCP_THROW;

	// Check against the magic 2 bytes.
	// The value of 'BM' in integer is 19778 (assuming little endian)
	if(fileHeader.bfType != 19778u) { st = Status::image_invalid_header; roEXCP_THROW; }
	if(infoHeader.biBitCount != 24) { st = Status::image_bmp_only_24bits_color_supported; roEXCP_THROW; }
	if(infoHeader.biCompression != 0) { st = Status::image_bmp_compression_not_supported; roEXCP_THROW; }

	width = infoHeader.biWidth;
	if(infoHeader.biHeight > 0) {
		height = infoHeader.biHeight;
		flipVertical = true;
	}
	else {
		height = -infoHeader.biHeight;
		flipVertical = false;
	}

	nextFun = &BmpLoader::initTexture;

roEXCP_CATCH
	roLog("error", "BmpLoader: Fail to load '%s', reason: %s\n", texture->uri().c_str(), st.c_str());
	nextFun = &BmpLoader::abort;

roEXCP_END
	return reSchedule(false, taskPool->mainThreadId());
}

void BmpLoader::initTexture(TaskPool* taskPool)
{
	if(roRDriverCurrentContext->driver->initTexture(texture->handle, width, height, 1, roRDriverTextureFormat_RGBA, roRDriverTextureFlag_None))
	{
		nextFun = &BmpLoader::loadPixelData;
		return reSchedule(false, ~taskPool->mainThreadId());
	}
	else
	{
		nextFun = &BmpLoader::abort;
		return reSchedule(false, taskPool->mainThreadId());
	}
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

void BmpLoader::loadPixelData(TaskPool* taskPool)
{
	Status st;

roEXCP_TRY
	if(!stream) { st = Status::pointer_is_null; roEXCP_THROW; }

	// Memory usage for one row of image
	const roSize rowByte = width * (sizeof(char) * 3);
	const roSize rowPadding = 4 - ((rowByte + 4) % 4);

	// If data not ready, give up in this round and do it again in next schedule
	if(fileSystem.readWillBlock(stream, pixelDataSize + rowPadding * height))
		return reSchedule();

	pixelData.resize(rowByte * height);

	if(pixelData.size() != rowByte * height) { st = Status::not_enough_memory; roEXCP_THROW; }

	char paddingBuf[4];

	// At this point we can read every pixel of the image
	for(unsigned h = 0; h<height; ++h) {
		// Bitmap file is differ from other image format like jpg and png that
		// the vertical scan line order is inverted.
		const unsigned invertedH = flipVertical ? height - 1 - h : h;

		roBytePtr p = pixelData.bytePtr() + (invertedH * rowByte);

		st = fileSystem.atomicRead(stream, p, rowByte);
		if(!st) roEXCP_THROW;

		// Consume any row padding
		st = fileSystem.atomicRead(stream, paddingBuf, rowPadding);
		if(!st) roEXCP_THROW;
	}

	// Convert BGR to RGBA
	Array<roUint8> tmpBuffer(width * height * 4);
	_bgrToRgba(pixelData.bytePtr(), tmpBuffer.bytePtr(), width, height);
	roSwap(pixelData, tmpBuffer);

	nextFun = &BmpLoader::commit;

roEXCP_CATCH
	roLog("error", "BmpLoader: Fail to load '%s', reason: %s\n", texture->uri().c_str(), st.c_str());
	nextFun = &BmpLoader::abort;

roEXCP_END
	return reSchedule(false, taskPool->mainThreadId());
}

void BmpLoader::commit(TaskPool* taskPool)
{
	if(roRDriverCurrentContext->driver->updateTexture(texture->handle, 0, 0, pixelData.bytePtr(), 0, NULL)) {
		texture->state = Resource::Loaded;
		delete this;
	}
	else {
		nextFun = &BmpLoader::abort;
		return reSchedule(false, taskPool->mainThreadId());
	}
}

void BmpLoader::abort(TaskPool* taskPool)
{
	texture->state = Resource::Aborted;
	delete this;
}

Resource* resourceCreateBmp(ResourceManager* mgr, const char* uri)
{
	return new Texture(uri);
}

bool resourceLoadBmp(ResourceManager* mgr, Resource* resource)
{
	Texture* texture = dynamic_cast<Texture*>(resource);
	if(!texture)
		return false;

	if(!texture->handle)
		texture->handle = roRDriverCurrentContext->driver->newTexture();

	TaskPool* taskPool = mgr->taskPool;
	BmpLoader* loaderTask = new BmpLoader(texture, mgr);
	texture->taskReady = texture->taskLoaded = taskPool->addFinalized(loaderTask, 0, 0, ~taskPool->mainThreadId());

	return true;
}

bool extMappingBmp(const char* uri, void*& createFunc, void*& loadFunc)
{
	if(!uriExtensionMatch(uri, ".bmp"))
		return false;

	createFunc = resourceCreateBmp;
	loadFunc = resourceLoadBmp;
	return true;
}

}	// namespace ro
