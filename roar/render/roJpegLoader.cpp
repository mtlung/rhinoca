#include "pch.h"
#include "roTexture.h"
#include "roRenderDriver.h"
#include "../base/roCpuProfiler.h"
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

struct JpegLoader : public Task
{
	JpegLoader(Texture* t, ResourceManager* mgr)
		: stream(NULL), texture(t), manager(mgr)
		, width(0), height(0)
		, pixelDataFormat(roRDriverTextureFormat_RGBA)
		, mappedPtr(NULL), mappedRowBytes(0)
		, decoder(NULL), jpegStream(NULL)
		, nextFun(&JpegLoader::loadHeader)
	{}

	~JpegLoader()
	{
		if(stream) fileSystem.closeFile(stream);
		delete decoder;
		delete jpegStream;
	}

	void run(TaskPool* taskPool) override;

	void loadHeader(TaskPool* taskPool);
	void initTexture(TaskPool* taskPool);
	void loadPixelData(TaskPool* taskPool);
	void commit(TaskPool* taskPool);
	void abort(TaskPool* taskPool);

	void* stream;
	TexturePtr texture;
	ResourceManager* manager;
	unsigned width, height;
	roRDriverTextureFormat pixelDataFormat;

	roByte* mappedPtr;
	roSize mappedRowBytes;

	Pjpeg_decoder decoder;
	Stream* jpegStream;

	void (JpegLoader::*nextFun)(TaskPool*);
};

void JpegLoader::run(TaskPool* taskPool)
{
	CpuProfilerScope cpuProfilerScope(__FUNCTION__);

	if(texture->state == Resource::Aborted || !taskPool->keepRun()) {
		nextFun = &JpegLoader::abort;

		if(taskPool->threadId() != taskPool->mainThreadId())
			return reSchedule(false, taskPool->mainThreadId());
	}

	(this->*nextFun)(taskPool);
}

void JpegLoader::loadHeader(TaskPool* taskPool)
{
	CpuProfilerScope cpuProfilerScope(__FUNCTION__);

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
	CpuProfilerScope cpuProfilerScope(__FUNCTION__);

roEXCP_TRY
	if(!roRDriverCurrentContext->driver->initTexture(texture->handle, width, height, 1, pixelDataFormat, roRDriverTextureFlag_None)) {
		roLog("error", "JpegLoader: Fail to initTexture '%s'\n", texture->uri().c_str());
		roEXCP_THROW;
	}

	roAssert(!mappedPtr);
	mappedPtr = (roByte*)roRDriverCurrentContext->driver->mapTexture(texture->handle, roRDriverMapUsage_Write, 0, 0, mappedRowBytes);

	if(!mappedPtr || !mappedRowBytes)
		roEXCP_THROW;

	nextFun = &JpegLoader::loadPixelData;
	return reSchedule(false, ~taskPool->mainThreadId());

roEXCP_CATCH
	nextFun = &JpegLoader::abort;
	return reSchedule(false, taskPool->mainThreadId());
roEXCP_END
}

void JpegLoader::loadPixelData(TaskPool* taskPool)
{
	CpuProfilerScope cpuProfilerScope(__FUNCTION__);

	Status st;

roEXCP_TRY
	void* Pscan_line_ofs = NULL;
	uint scan_line_len = 0;
	int c = decoder->get_num_components();

	roUint8* p = mappedPtr;
	if(!p)
		roEXCP_THROW;

	while(true) {
		int result = decoder->decode(&Pscan_line_ofs, &scan_line_len);
		if(result == JPGD_OKAY) {
			memcpy(p, Pscan_line_ofs, scan_line_len);

			roUint8* end = p + mappedRowBytes;

			// Assign alpha to 1 for incoming is RGB
			if(c == 3) for(; p < end; p += 4)
				p[3] = TypeOf<roUint8>::valueMax();

			p = end;
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
	CpuProfilerScope cpuProfilerScope(__FUNCTION__);

	if(mappedPtr) {
		roRDriverCurrentContext->driver->unmapTexture(texture->handle, 0, 0);
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
	if(mappedPtr)
		roRDriverCurrentContext->driver->unmapTexture(texture->handle, 0, 0);

	mappedPtr = NULL;
	texture->state = Resource::Aborted;
	delete this;
}

Resource* resourceCreateJpeg(ResourceManager* mgr, const char* uri)
{
	return new Texture(uri);
}

bool resourceLoadJpeg(ResourceManager* mgr, Resource* resource)
{
	Texture* texture = dynamic_cast<Texture*>(resource);
	if(!texture)
		return false;

	if(!texture->handle)
		texture->handle = roRDriverCurrentContext->driver->newTexture();

	TaskPool* taskPool = mgr->taskPool;
	JpegLoader* loaderTask = new JpegLoader(texture, mgr);
	texture->taskReady = texture->taskLoaded = taskPool->addFinalized(loaderTask, 0, 0, ~taskPool->mainThreadId());

	return true;
}

bool extMappingJpeg(const char* uri, void*& createFunc, void*& loadFunc)
{
	if(!uriExtensionMatch(uri, ".jpg") && !uriExtensionMatch(uri, ".jpeg"))
		return false;

	createFunc = resourceCreateJpeg;
	loadFunc = resourceLoadJpeg;
	return true;
}

}	// namespace ro
