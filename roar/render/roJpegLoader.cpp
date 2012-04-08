#include "pch.h"
#include "roTexture.h"
#include "roRenderDriver.h"
#include "../base/roFileSystem.h"
#include "../base/roLog.h"
#include "../base/roMemory.h"
#include "../base/roTypeOf.h"
#include "../../thirdparty/SmallJpeg/jpegdecoder.h"

#if roCOMPILER_VC
#	pragma comment(lib, "SmallJpeg")
#endif

namespace ro {

enum TextureLoadingState {
	TextureLoadingState_LoadHeader,
	TextureLoadingState_InitTexture,
	TextureLoadingState_LoadPixelData,
	TextureLoadingState_Commit,
	TextureLoadingState_Finish,
	TextureLoadingState_Abort
};

// TODO: The class jpeg_decoder_stream didn't flexible enough to take advantage of our async file system
class Stream : public jpeg_decoder_stream
{
public:
	explicit Stream(void* f) : file(f) {}

	int read(uchar* Pbuf, int max_bytes_to_read, bool* Peof_flag)
	{
		int readCount = (int)fileSystem.read(file, (char*)Pbuf, max_bytes_to_read);
		*Peof_flag = (readCount == 0);

		return readCount;
	}

	void* file;
};	// Stream

Resource* resourceCreateJpeg(const char* uri, ResourceManager* mgr)
{
	if(uriExtensionMatch(uri, ".jpg") || uriExtensionMatch(uri, ".jpeg"))
		return new Texture(uri);
	return NULL;
}

class JpegLoader : public Task
{
public:
	JpegLoader(Texture* t, ResourceManager* mgr)
		: stream(NULL), texture(t), manager(mgr)
		, width(0), height(0)
		, pixelData(NULL), pixelDataSize(0), rowBytes(0), pixelDataFormat(roRDriverTextureFormat_RGBA)
		, decoder(NULL), jpegStream(NULL)
		, textureLoadingState(TextureLoadingState_LoadHeader)
	{}

	~JpegLoader()
	{
		if(stream) fileSystem.closeFile(stream);
		roFree(pixelData);
		delete decoder;
		delete jpegStream;
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
	roSize rowBytes;
	roRDriverTextureFormat pixelDataFormat;

	Pjpeg_decoder decoder;
	Stream* jpegStream;

	TextureLoadingState textureLoadingState;
};

void JpegLoader::run(TaskPool* taskPool)
{
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

void JpegLoader::loadHeader(TaskPool* taskPool)
{
	Status st;
	if(!stream) st = fileSystem.openFile(texture->uri(), stream);
	if(!st) {
		roLog("error", "JpegLoader: Fail to open file '%s', reason: %s\n", texture->uri().c_str(), st.c_str());
		goto Abort;
	}

	decoder = new jpeg_decoder(jpegStream = new Stream(stream), true);

	if(decoder->get_error_code() != JPGD_OKAY) {
		roLog("error", "JpegLoader: load error, operation aborted\n");
		goto Abort;
	}

	if(decoder->begin() != JPGD_OKAY) {
		roLog("error", "JpegLoader: load error, operation aborted\n");
		goto Abort;
	}

	int c = decoder->get_num_components();
	if(c == 1)
		pixelDataFormat = roRDriverTextureFormat_R;
	else if(c == 3)
		pixelDataFormat = roRDriverTextureFormat_RGBA;	// Note that the source format is 4 byte even c == 3
	else {
		roLog("error", "JpegLoader: image with number of color component equals to %i is not supported, operation aborted\n", c);
		goto Abort;
	}

	width = decoder->get_width();
	height = decoder->get_height();

	textureLoadingState = TextureLoadingState_InitTexture;
	return reSchedule(false, taskPool->mainThreadId());

Abort:
	textureLoadingState = TextureLoadingState_Abort;
}

void JpegLoader::initTexture(TaskPool* taskPool)
{
	if(roRDriverCurrentContext->driver->initTexture(texture->handle, width, height, 1, pixelDataFormat, roRDriverTextureFlag_None))
	{
		textureLoadingState = TextureLoadingState_LoadPixelData;
		return reSchedule(false, ~taskPool->mainThreadId());
	}
	else
		goto Abort;

Abort:
	textureLoadingState = TextureLoadingState_Abort;
}

void JpegLoader::loadPixelData(TaskPool* taskPool)
{
	roAssert(!pixelData);
	rowBytes = decoder->get_bytes_per_scan_line();
	pixelDataSize = rowBytes * height;
	pixelData = roMalloc(pixelDataSize);

	if(!pixelData) {
		roLog("error", "JpegLoader: Corruption of file or not enough memory, operation aborted\n");
		goto Abort;
	}

	void* Pscan_line_ofs = NULL;
	uint scan_line_len = 0;
	int c = decoder->get_num_components();

	char* p = pixelData;
	while(true) {
		int result = decoder->decode(&Pscan_line_ofs, &scan_line_len);
		if(result == JPGD_OKAY) {
			memcpy(p, Pscan_line_ofs, scan_line_len);

			// Assign alpha to 1 for incoming is RGB
			if(c == 3) for(char* end = p + scan_line_len; p < end; p += 4)
				p[3] = TypeOf<char>::valueMax();
			else
				p += decoder->get_bytes_per_scan_line();

			continue;
		}
		else if(result == JPGD_DONE)
			break;
		else
			goto Abort;
	}

	textureLoadingState = TextureLoadingState_Commit;
	return reSchedule(false, taskPool->mainThreadId());

Abort:
	textureLoadingState = TextureLoadingState_Abort;
}

void JpegLoader::commit(TaskPool* taskPool)
{
	if(roRDriverCurrentContext->driver->updateTexture(texture->handle, 0, 0, pixelData, 0, NULL)) {
		textureLoadingState = TextureLoadingState_Finish;
		return;
	}
	else
		goto Abort;

Abort:
	textureLoadingState = TextureLoadingState_Abort;
}

bool resourceLoadJpeg(Resource* resource, ResourceManager* mgr)
{
	if(!uriExtensionMatch(resource->uri(), ".jpg") && !uriExtensionMatch(resource->uri(), ".jpeg")) return false;

	TaskPool* taskPool = mgr->taskPool;

	Texture* texture = dynamic_cast<Texture*>(resource);
	texture->handle = roRDriverCurrentContext->driver->newTexture();

	JpegLoader* loaderTask = new JpegLoader(texture, mgr);
	texture->taskReady = texture->taskLoaded = taskPool->addFinalized(loaderTask, 0, 0, ~taskPool->mainThreadId());

	return true;
}

}	// namespace ro
