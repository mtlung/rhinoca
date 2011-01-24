#include "pch.h"
#include "../render/texture.h"
#include "../platform.h"
#include "../../thirdparty/png/png.h"

#ifdef RHINOCA_VC
#	pragma comment(lib, "png")
#	pragma comment(lib, "zlib")
#endif

// Reading of png files using the libpng
// The code is base on the example provided in libpng
// More information can be found in
// http://www.libpng.org/
// http://www.libpng.org/pub/png/libpng-1.2.5-manual.html

using namespace Render;

namespace Loader {

Resource* createPng(const char* uri, ResourceManager* mgr)
{
	if(uriExtensionMatch(uri, ".png"))
		return new Render::Texture(uri);
	return NULL;
}

class PngLoader : public Task
{
public:
	PngLoader(Texture* t, ResourceManager* mgr);

	~PngLoader();

	virtual void run(TaskPool* taskPool);

	void load(TaskPool* taskPool);
	void commit(TaskPool* taskPool);

	void onPngInfoReady();

	Texture* texture;
	ResourceManager* manager;
	rhuint width, height;
	char* pixelData;
	rhuint pixelDataSize;
	int pixelDataFormat;

	png_structp png_ptr;
	png_infop info_ptr;
	png_uint_32 rowBytes;	// Number of byte per row of image data
	int currentPass;		// For keep tracking when a new pass is loaded
	bool _aborted;
	bool _loadFinished;

	TaskPool* _taskPool;
};

static void info_callback(png_structp png_ptr, png_infop)
{
	PngLoader* impl = reinterpret_cast<PngLoader*>(png_get_progressive_ptr(png_ptr));
	impl->onPngInfoReady();
}

void PngLoader::onPngInfoReady()
{
	Rhinoca* rh = manager->rhinoca;

	// Query the info structure.
	int bit_depth, color_type, interlace_method;
	png_get_IHDR(png_ptr, info_ptr,
		(png_uint_32*)(&width), (png_uint_32*)(&height),
		&bit_depth, &color_type, &interlace_method, NULL, NULL);

	rowBytes = info_ptr->rowbytes;

	switch(color_type) {
	case PNG_COLOR_TYPE_RGB:
		pixelDataFormat = Texture::RGB;
		break;
	case PNG_COLOR_TYPE_RGB_ALPHA:
		pixelDataFormat = Texture::RGBA;
		break;
	case PNG_COLOR_TYPE_GRAY:
		print(rh, "PngLoader: gray scale image is not yet supported, operation aborted");
	case PNG_COLOR_TYPE_GRAY_ALPHA:
		print(rh, "PngLoader: gray scale image is not yet supported, operation aborted");
	case PNG_COLOR_TYPE_PALETTE:	// Color palette is not supported
		print(rh, "PngLoader: image using color palette is not supported, operation aborted");
	default:
		_aborted = true;
		break;
	}

	// We'll let libpng expand interlaced images.
	if(interlace_method == PNG_INTERLACE_ADAM7) {
		int number_of_passes = png_set_interlace_handling(png_ptr);
		(void)number_of_passes;
	}

	png_read_update_info(png_ptr, info_ptr);

	ASSERT(!pixelData);
	pixelDataSize = rowBytes * height;
	pixelData = (char*)rhinoca_malloc(pixelDataSize);
}

// This function is called for every pass when each row of image data is complete.
static void row_callback(png_structp png_ptr, png_bytep new_row, png_uint_32 row_num, int pass)
{
	PngLoader* impl = reinterpret_cast<PngLoader*>(png_get_progressive_ptr(png_ptr));
	ASSERT(impl->pixelData);

	// Have libpng either combine the new row data with the existing row data
	// from previous passes (if interlaced) or else just copy the new row
	// into the main program's image buffer
	png_progressive_combine_row(png_ptr, (png_byte*)(impl->pixelData + row_num * impl->rowBytes), new_row);

	// Only change the loading state after a pass is finished
	if(pass > impl->currentPass) {
		impl->currentPass = pass;
		// TODO: Implement progressive loading
//		impl->mLoadingState = PartialLoaded;
	}
}

static void end_callback(png_structp png_ptr, png_infop)
{
	PngLoader* impl = reinterpret_cast<PngLoader*>(png_get_progressive_ptr(png_ptr));
	impl->_loadFinished = true;
}

PngLoader::PngLoader(Texture* t, ResourceManager* mgr)
	: texture(t), manager(mgr)
	, width(0), height(0)
	, pixelData(NULL), pixelDataSize(0), pixelDataFormat(0)
	, png_ptr(NULL), info_ptr(NULL)
	, rowBytes(0), currentPass(0)
	, _aborted(false)
	, _loadFinished(false)
	, _taskPool(NULL)
{
	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	ASSERT(png_ptr);

	info_ptr = png_create_info_struct(png_ptr);
	ASSERT(info_ptr);

	// Setup the callback that will be called during the data processing.
	png_set_progressive_read_fn(png_ptr, (void*)this, info_callback, row_callback, end_callback);
}

PngLoader::~PngLoader()
{
	rhinoca_free(pixelData);
	// libpng will handle for null input pointers
	png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
}

void PngLoader::run(TaskPool* taskPool)
{
	if(!texture->scratch)
		load(taskPool);
	else
		commit(taskPool);
}

void PngLoader::load(TaskPool* taskPool)
{
	texture->scratch = this;
	int tId = TaskPool::threadId();
	Rhinoca* rh = manager->rhinoca;

	void* f = io_open(rh, texture->uri(), tId);
	if(!f) goto Abort;

	char buff[1024*8];
	unsigned readCount = 0;

	// Jump to here if any error occur in png_process_data
	if(setjmp(png_jmpbuf(png_ptr)))
		goto Abort;

	do {
		readCount = (unsigned)io_read(f, buff, sizeof(buff), tId);
		png_process_data(png_ptr, info_ptr, (png_bytep)buff, readCount);

		if(_aborted) goto Abort;
	} while(readCount > 0 && !_loadFinished);

	io_close(f, tId);
	return;

Abort:
	if(f) io_close(f, tId);
	texture->state = Resource::Aborted;
}

void PngLoader::commit(TaskPool* taskPool)
{
	ASSERT(texture->scratch == this);
	texture->scratch = NULL;

	if(texture->create(width, height, pixelData, pixelDataSize, pixelDataFormat))
		texture->state = Resource::Loaded;
	else
		texture->state = Resource::Aborted;

	delete this;
}

bool loadPng(Resource* resource, ResourceManager* mgr)
{
	if(!uriExtensionMatch(resource->uri(), ".png")) return false;

	TaskPool* taskPool = mgr->taskPool;

	Texture* texture = dynamic_cast<Texture*>(resource);

	PngLoader* loaderTask = new PngLoader(texture, mgr);
	texture->taskReady = taskPool->addFinalized(loaderTask);
	texture->taskLoaded = taskPool->addFinalized(loaderTask, 0, texture->taskReady, taskPool->mainThreadId());

	return true;
}

}	// namespace Loader
