#include "pch.h"
#include "../render/texture.h"
#include "../platform.h"
#include "../../thirdparty/SmallJpeg/jpegdecoder.h"

#ifdef RHINOCA_VC
#	pragma comment(lib, "SmallJpeg")
#endif

using namespace Render;

namespace Loader {

class Stream : public jpeg_decoder_stream
{
public:
	explicit Stream(void* f, int tId) : file(f), threadId(tId) {}

	int read(uchar* Pbuf, int max_bytes_to_read, bool* Peof_flag)
	{
		int readCount = (int)io_read(file, (char*)Pbuf, max_bytes_to_read, threadId);
		*Peof_flag = (readCount == 0);

		return readCount;
	}

	void* file;
	int threadId;
};	// Stream

Resource* createJpeg(const char* uri, ResourceManager* mgr)
{
	if(uriExtensionMatch(uri, ".jpg") || uriExtensionMatch(uri, ".jpeg"))
		return new Render::Texture(uri);
	return NULL;
}

class JpegLoader : public Task
{
public:
	JpegLoader(Texture* t, ResourceManager* mgr)
		: texture(t), manager(mgr)
		, width(0), height(0)
		, pixelData(NULL), pixelDataSize(0), rowBytes(0), pixelDataFormat(Driver::RGBA)
		, _decoder(NULL), _stream(NULL)
	{}

	~JpegLoader()
	{
		rhinoca_free(pixelData);
		delete _decoder;
		delete _stream;
	}

	virtual void run(TaskPool* taskPool);

	void load(TaskPool* taskPool);
	void commit(TaskPool* taskPool);

	Texture* texture;
	ResourceManager* manager;
	rhuint width, height;
	char* pixelData;
	rhuint pixelDataSize;
	rhuint rowBytes;
	Texture::Format pixelDataFormat;

	Pjpeg_decoder _decoder;
	Stream* _stream;
};

void JpegLoader::run(TaskPool* taskPool)
{
	if(!texture->scratch)
		load(taskPool);
	else
		commit(taskPool);
}

void JpegLoader::load(TaskPool* taskPool)
{
	texture->scratch = this;
	int tId = TaskPool::threadId();
	Rhinoca* rh = manager->rhinoca;

	void* f = io_open(rh, texture->uri(), tId);
	if(!f) goto Abort;

	_decoder = new jpeg_decoder(_stream = new Stream(f, tId), true);

	if(_decoder->get_error_code() != JPGD_OKAY) {
		print(rh, "JpegLoader: load error, operation aborted");
		goto Abort;
	}

	if(_decoder->begin() != JPGD_OKAY) {
		print(rh, "JpegLoader: load error, operation aborted");
		goto Abort;
	}

	int c = _decoder->get_num_components();
	if(c == 1)
		pixelDataFormat = Driver::LUMINANCE;
	else if(c == 3)
		pixelDataFormat = Driver::RGBA;	// Note that the source format is 4 byte even c == 3
	else {
		print(rh, "JpegLoader: image with number of color component equals to %i is not supported, operation aborted", c);
		goto Abort;
	}

	width = _decoder->get_width();
	height = _decoder->get_height();
	rowBytes = _decoder->get_bytes_per_scan_line();

	ASSERT(!pixelData);
	pixelDataSize = rowBytes * height;
	pixelData = (char*)rhinoca_malloc(pixelDataSize);

	void* Pscan_line_ofs = NULL;
	uint scan_line_len = 0;

	char* p = pixelData;
	while(true) {
		int result = _decoder->decode(&Pscan_line_ofs, &scan_line_len);
		if(result == JPGD_OKAY) {
			memcpy(p, Pscan_line_ofs, scan_line_len);

			// Assign alpha to 1 for incomming is RGB
			if(c == 3) for(char* end = p + scan_line_len; p < end; p += 4)
				p[3] = (char)255;
			else
				p += _decoder->get_bytes_per_scan_line();

			continue;
		}
		else if(result == JPGD_DONE)
			break;
		else
			goto Abort;
	}

	io_close(f, tId);
	return;

Abort:
	if(f) io_close(f, tId);
	texture->state = Resource::Aborted;
}

void JpegLoader::commit(TaskPool* taskPool)
{
	ASSERT(texture->scratch == this);
	texture->scratch = NULL;

	if(texture->create(width, height, Driver::ANY, pixelData, pixelDataSize, pixelDataFormat))
		texture->state = Resource::Loaded;
	else
		texture->state = Resource::Aborted;

	delete this;
}

bool loadJpeg(Resource* resource, ResourceManager* mgr)
{
	if(!uriExtensionMatch(resource->uri(), ".jpg") && !uriExtensionMatch(resource->uri(), ".jpeg")) return false;

	TaskPool* taskPool = mgr->taskPool;

	Texture* texture = dynamic_cast<Texture*>(resource);

	JpegLoader* loaderTask = new JpegLoader(texture, mgr);
	texture->taskReady = taskPool->addFinalized(loaderTask);
	texture->taskLoaded = taskPool->addFinalized(loaderTask, 0, texture->taskReady, taskPool->mainThreadId());

	return true;
}

}	// namespace Loader
