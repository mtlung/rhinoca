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

enum TextureLoadingState {
	TextureLoadingState_LoadHeader,
	TextureLoadingState_InitTexture,
	TextureLoadingState_LoadPixelData,
	TextureLoadingState_Commit,
	TextureLoadingState_Finish,
	TextureLoadingState_Abort
};

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
		, flipVertical(false)
		, textureLoadingState(TextureLoadingState_LoadHeader)
	{}

	~BmpLoader()
	{
		if(stream) fileSystem.closeFile(stream);
		roFree(pixelData);
	}

	override void run(TaskPool* taskPool);

protected:
	void loadHeader(TaskPool* taskPool);
	void initTexture(TaskPool* taskPool);
	void loadPixelData(TaskPool* taskPool);
	void commit(TaskPool* taskPool);

	void* stream;
	Texture* texture;
	ResourceManager* manager;
	unsigned width, height;
	roBytePtr pixelData;
	roSize pixelDataSize;

	bool flipVertical;
	BITMAPFILEHEADER fileHeader;
	BITMAPINFOHEADER infoHeader;

	TextureLoadingState textureLoadingState;
};

void BmpLoader::run(TaskPool* taskPool)
{
	if(texture->state == Resource::Aborted)
		textureLoadingState = TextureLoadingState_Abort;

	if(textureLoadingState == TextureLoadingState_LoadHeader)
		loadHeader(taskPool);
	else if(textureLoadingState == TextureLoadingState_InitTexture)
		initTexture(taskPool);
	else if(textureLoadingState == TextureLoadingState_LoadPixelData)
		loadPixelData(taskPool);
	else if(textureLoadingState == TextureLoadingState_Commit)
		commit(taskPool);

	if(textureLoadingState == TextureLoadingState_Finish) {
		texture->state = Resource::Loaded;
		delete this;
		return;
	}

	if(textureLoadingState == TextureLoadingState_Abort) {
		texture->state = Resource::Aborted;
		delete this;
		return;
	}
}

void BmpLoader::loadHeader(TaskPool* taskPool)
{
	Status st;
	if(!stream) st = fileSystem.openFile(texture->uri(), stream);
	if(!st) {
		roLog("error", "BmpLoader: Fail to open file '%s', reason: %s\n", texture->uri().c_str(), st.c_str());
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

	textureLoadingState = TextureLoadingState_InitTexture;
	return reSchedule(false, taskPool->mainThreadId());

Abort:
	textureLoadingState = TextureLoadingState_Abort;
}

void BmpLoader::initTexture(TaskPool* taskPool)
{
	if(roRDriverCurrentContext->driver->initTexture(texture->handle, width, height, 1, roRDriverTextureFormat_RGBA, roRDriverTextureFlag_None))
	{
		textureLoadingState = TextureLoadingState_LoadPixelData;
		return reSchedule(false, ~taskPool->mainThreadId());
	}
	else
		textureLoadingState = TextureLoadingState_Abort;
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
	roBytePtr tmpBuffer = NULL;

	if(!stream) goto Abort;

	// Memory usage for one row of image
	const roSize rowByte = width * (sizeof(char) * 3);
	const roSize rowPadding = 4 - ((rowByte + 4) % 4);

	roAssert(!pixelData);
	pixelDataSize = rowByte * height;

	// If data not ready, give up in this round and do it again in next schedule
	if(!fileSystem.readReady(stream, pixelDataSize + rowPadding * height))
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

		// Consume any row padding
		roVerify(fileSystem.read(stream, paddingBuf, rowPadding) == rowPadding);
	}

	// Convert BGR to RGBA
	tmpBuffer = roMalloc(width * height * 4);
	_bgrToRgba(pixelData, tmpBuffer, width, height);
	roSwap(pixelData, tmpBuffer);
	roFree(tmpBuffer);

	textureLoadingState = TextureLoadingState_Commit;
	return reSchedule(false, taskPool->mainThreadId());

Abort:
	textureLoadingState = TextureLoadingState_Abort;
}

void BmpLoader::commit(TaskPool* taskPool)
{
	if(roRDriverCurrentContext->driver->updateTexture(texture->handle, 0, 0, pixelData, 0, NULL))
		textureLoadingState = TextureLoadingState_Finish;
	else
		textureLoadingState = TextureLoadingState_Abort;
}

bool resourceLoadBmp(Resource* resource, ResourceManager* mgr)
{
	if(!uriExtensionMatch(resource->uri(), ".bmp")) return false;

	TaskPool* taskPool = mgr->taskPool;

	Texture* texture = dynamic_cast<Texture*>(resource);
	texture->handle = roRDriverCurrentContext->driver->newTexture();

	BmpLoader* loaderTask = new BmpLoader(texture, mgr);
	texture->taskReady = texture->taskLoaded = taskPool->addFinalized(loaderTask, 0, 0, ~taskPool->mainThreadId());

	return true;
}

}	// namespace ro
