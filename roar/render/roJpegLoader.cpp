#include "pch.h"
#include "roTexture.h"
#include "roRenderDriver.h"
#include "../base/roFileSystem.h"
#include "../base/roLog.h"
#include "../base/roMemory.h"
#include "../base/roTypeCast.h"
#include "../base/roTypeOf.h"
#include "../../thirdparty/SmallJpeg/jpegdecoder.h"

#if roCOMPILER_VC
#	pragma comment(lib, "SmallJpeg")
#endif

namespace ro {

// TODO: The class jpeg_decoder_stream didn't flexible enough to take advantage of our async file system
class Stream : public jpeg_decoder_stream
{
public:
	explicit Stream(void* f) : file(f) {}

	int read(uchar* Pbuf, int max_bytes_to_read, bool* Peof_flag)
	{
		roUint64 bytesRead = 0;
		fileSystem.read(file, (char*)Pbuf, max_bytes_to_read, bytesRead);
		*Peof_flag = (bytesRead == 0);

		return num_cast<int>(bytesRead);
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
		, nextFun(&JpegLoader::loadHeader)
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
	void abort(TaskPool* taskPool);

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

	void (JpegLoader::*nextFun)(TaskPool*);
};

void JpegLoader::run(TaskPool* taskPool)
{
	if(texture->state == Resource::Aborted)
		nextFun = &JpegLoader::abort;

	(this->*nextFun)(taskPool);
}

void JpegLoader::loadHeader(TaskPool* taskPool)
{
	Status st;

roEXCP_TRY
	if(!stream) st = fileSystem.openFile(texture->uri(), stream);
	if(!st) roEXCP_THROW;

	decoder = new jpeg_decoder(jpegStream = new Stream(stream), true);

	if(decoder->get_error_code() != JPGD_OKAY) { st = Status::image_jpeg_error; roEXCP_THROW; }
	if(decoder->begin() != JPGD_OKAY) { st = Status::image_jpeg_error; roEXCP_THROW; }

	int c = decoder->get_num_components();
	if(c == 1)
		pixelDataFormat = roRDriverTextureFormat_L;
	else if(c == 3)
		pixelDataFormat = roRDriverTextureFormat_RGBA;	// Note that the source format is 4 byte even c == 3
	else {
		st = Status::image_jpeg_channel_count_not_supported; roEXCP_THROW;
	}

	width = decoder->get_width();
	height = decoder->get_height();

	nextFun = &JpegLoader::initTexture;

roEXCP_CATCH
	roLog("error", "JpegLoader: Fail to load '%s', reason: %s\n", texture->uri().c_str(), st.c_str());
	nextFun = &JpegLoader::abort;

roEXCP_END
	return reSchedule(false, taskPool->mainThreadId());
}

void JpegLoader::initTexture(TaskPool* taskPool)
{
	if(roRDriverCurrentContext->driver->initTexture(texture->handle, width, height, 1, pixelDataFormat, roRDriverTextureFlag_None))
	{
		nextFun = &JpegLoader::loadPixelData;
		return reSchedule(false, ~taskPool->mainThreadId());
	}
	else
	{
		nextFun = &JpegLoader::abort;
		return reSchedule(false, taskPool->mainThreadId());
	}
}

void JpegLoader::loadPixelData(TaskPool* taskPool)
{
	Status st;

roEXCP_TRY
	roAssert(!pixelData);
	rowBytes = decoder->get_bytes_per_scan_line();
	pixelDataSize = rowBytes * height;
	pixelData = roMalloc(pixelDataSize);

	if(!pixelData) { st = Status::not_enough_memory; roEXCP_THROW; }

	void* Pscan_line_ofs = NULL;
	uint scan_line_len = 0;
	int c = decoder->get_num_components();

	unsigned char* p = pixelData.cast<unsigned char>();
	while(true) {
		int result = decoder->decode(&Pscan_line_ofs, &scan_line_len);
		if(result == JPGD_OKAY) {
			memcpy(p, Pscan_line_ofs, scan_line_len);

			// Assign alpha to 1 for incoming is RGB
			if(c == 3) for(unsigned char* end = p + scan_line_len; p < end; p += 4)
				p[3] = TypeOf<unsigned char>::valueMax();
			else
				p += decoder->get_bytes_per_scan_line();

			continue;
		}
		else if(result == JPGD_DONE)
			break;
		else {
			st = Status::image_jpeg_error; roEXCP_THROW;
		}
	}

	nextFun = &JpegLoader::commit;

roEXCP_CATCH
	roLog("error", "JpegLoader: Fail to load '%s', reason: %s\n", texture->uri().c_str(), st.c_str());
	nextFun = &JpegLoader::abort;

roEXCP_END
	return reSchedule(false, taskPool->mainThreadId());
}

void JpegLoader::commit(TaskPool* taskPool)
{
	if(roRDriverCurrentContext->driver->updateTexture(texture->handle, 0, 0, pixelData, 0, NULL)) {
		texture->state = Resource::Loaded;
		delete this;
	}
	else {
		nextFun = &JpegLoader::abort;
		return reSchedule(false, taskPool->mainThreadId());
	}
}

void JpegLoader::abort(TaskPool* taskPool)
{
	texture->state = Resource::Aborted;
	delete this;
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
