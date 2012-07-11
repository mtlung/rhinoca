#include "pch.h"
#include "roTexture.h"
#include "roRenderDriver.h"
#include "../base/roCpuProfiler.h"
#include "../base/roFileSystem.h"
#include "../base/roLog.h"
#include "../base/roMemory.h"
#include "../base/roTypeCast.h"
#include "../../thirdParty/png/png.h"

#if roCOMPILER_VC
#	pragma comment(lib, "png")
#	pragma comment(lib, "zlib")
#	pragma warning(disable:4611)
#endif

// Reading of png files using the libpng
// The code is base on the example provided in libpng
// More information can be found in
// http://www.libpng.org/
// http://www.libpng.org/pub/png/libpng-1.2.5-manual.html

namespace ro {

struct PngLoader : public Task
{
	PngLoader(Texture* t, ResourceManager* mgr);

	~PngLoader()
	{
		if(stream) fileSystem.closeFile(stream);
		// libpng will handle for null input pointers
		png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
	}

	override void run(TaskPool* taskPool);

	void processData(TaskPool* taskPool);
	void loadHeader();
	void initTexture(TaskPool* taskPool);
	void commit(TaskPool* taskPool);
	void abort(TaskPool* taskPool);

	void* stream;
	TexturePtr texture;
	ResourceManager* manager;
	unsigned width, height;
	Array<png_byte> pixelData;
	roSize rowBytes;
	roRDriverTextureFormat pixelDataFormat;

	png_infop info_ptr;
	png_structp png_ptr;
	int currentPass;		// For keep tracking when a new pass is loaded

	void (PngLoader::*nextFun)(TaskPool*);
};

static void info_callback(png_structp png_ptr, png_infop)
{
	PngLoader* impl = reinterpret_cast<PngLoader*>(png_get_progressive_ptr(png_ptr));
	impl->loadHeader();
}

// This function is called for every pass when each row of image data is complete.
static void row_callback(png_structp png_ptr, png_bytep new_row, png_uint_32 row_num, int pass)
{
	PngLoader* impl = reinterpret_cast<PngLoader*>(png_get_progressive_ptr(png_ptr));

	// Have libpng either combine the new row data with the existing row data
	// from previous passes (if interlaced) or else just copy the new row
	// into the main program's image buffer
	png_progressive_combine_row(png_ptr, &impl->pixelData[row_num * impl->rowBytes], new_row);

	// Only change the loading state after a pass is finished
	if(pass > impl->currentPass) {
		impl->currentPass = pass;
		// TODO: Implement progressive loading
		//impl->mLoadingState = PartialLoaded;
	}
}

static void _rgbToRgba(roBytePtr src, roBytePtr dst, unsigned width, unsigned height)
{
	for(unsigned h=0; h<height; ++h) for(unsigned w=0; w<width; ++w)
	{
		dst[0] = src[0];
		dst[1] = src[1];
		dst[2] = src[2];
		dst[3] = roUint8(-1);

		src += 3;
		dst += 4;
	}
}

static void end_callback(png_structp png_ptr, png_infop)
{
	PngLoader* impl = reinterpret_cast<PngLoader*>(png_get_progressive_ptr(png_ptr));
	impl->nextFun = &PngLoader::commit;

	if(impl->rowBytes / impl->width == 3) {
		Array<png_byte> tmpBuf(impl->height * impl->width * 4);
		_rgbToRgba(impl->pixelData.bytePtr(), tmpBuf.bytePtr(), impl->width, impl->height);
		impl->pixelData.swap(tmpBuf);
	}
}

PngLoader::PngLoader(Texture* t, ResourceManager* mgr)
	: stream(NULL), texture(t), manager(mgr)
	, width(0), height(0)
	, rowBytes(0), pixelDataFormat(roRDriverTextureFormat_RGBA)
	, info_ptr(NULL), png_ptr(NULL)
	, currentPass(0)
	, nextFun(&PngLoader::processData)
{
	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	roAssert(png_ptr);

	info_ptr = png_create_info_struct(png_ptr);
	roAssert(info_ptr);

	// Setup the callback that will be called during the data processing.
	png_set_progressive_read_fn(png_ptr, (void*)this, info_callback, row_callback, end_callback);
}

void PngLoader::run(TaskPool* taskPool)
{
	if(texture->state == Resource::Aborted || !taskPool->keepRun())
		nextFun = &PngLoader::abort;

	(this->*nextFun)(taskPool);
}

void PngLoader::loadHeader()
{
	// Query the info structure.
	int bit_depth, color_type, interlace_method;
	png_get_IHDR(png_ptr, info_ptr,
		(png_uint_32*)(&width), (png_uint_32*)(&height),
		&bit_depth, &color_type, &interlace_method, NULL, NULL);

	switch(color_type) {
	case PNG_COLOR_TYPE_RGB:
		pixelDataFormat = roRDriverTextureFormat_RGBA;
		break;
	case PNG_COLOR_TYPE_RGB_ALPHA:
		pixelDataFormat = roRDriverTextureFormat_RGBA;
		break;
	case PNG_COLOR_TYPE_GRAY:
		pixelDataFormat = roRDriverTextureFormat_L;
		break;
	case PNG_COLOR_TYPE_GRAY_ALPHA:
		pixelDataFormat = roRDriverTextureFormat_A;
		break;
	case PNG_COLOR_TYPE_PALETTE:
		png_set_palette_to_rgb(png_ptr);
		pixelDataFormat = roRDriverTextureFormat_RGBA;
		break;
	default:
		goto Abort;
	}

	// We'll let libpng expand interlaced images.
	if(interlace_method == PNG_INTERLACE_ADAM7) {
		int number_of_passes = png_set_interlace_handling(png_ptr);
		(void)number_of_passes;
	}

	png_read_update_info(png_ptr, info_ptr);
	rowBytes = info_ptr->rowbytes;

	pixelData.resize(rowBytes * height);

	nextFun = &PngLoader::initTexture;
	return;

Abort:
	nextFun = &PngLoader::abort;
}

void PngLoader::initTexture(TaskPool* taskPool)
{
	if(roRDriverCurrentContext->driver->initTexture(texture->handle, width, height, 1, pixelDataFormat, roRDriverTextureFlag_None))
	{
		texture->width = width;
		texture->height = height;
		nextFun = &PngLoader::processData;
		return reSchedule(false, ~taskPool->mainThreadId());
	}
	else
	{
		nextFun = &PngLoader::abort;
		return reSchedule(false, taskPool->mainThreadId());
	}
}

void PngLoader::processData(TaskPool* taskPool)
{
	CpuProfilerScope cpuProfilerScope(__FUNCTION__);

	char buff[1024*8];

	if(setjmp(png_jmpbuf(png_ptr)))
		nextFun = &PngLoader::abort;
	else do
	{
		Status st = Status::ok;
		if(!stream) st = fileSystem.openFile(texture->uri(), stream);
		if(!st) {
			roLog("error", "PngLoader: Fail to open file '%s', reason: %s\n", texture->uri().c_str(), st.c_str());
			nextFun = &PngLoader::abort;
			break;
		}

		if(fileSystem.readWillBlock(stream, sizeof(buff))) {
			// Re-schedule the load operation
			return reSchedule(false, ~taskPool->mainThreadId());
		}

		roUint64 bytesRead = 0;
		st = fileSystem.read(stream, buff, sizeof(buff), bytesRead);
		if(!st || bytesRead == 0) {
			nextFun = &PngLoader::abort;
			break;
		}
		png_process_data(png_ptr, info_ptr, (png_bytep)buff, num_cast<png_size_t>(bytesRead));
	} while(nextFun == &PngLoader::processData);

	return reSchedule(false, taskPool->mainThreadId());
}

void PngLoader::commit(TaskPool* taskPool)
{
roEXCP_TRY
	// All the header and pixel data may be finished in one go, we might need to call initTexture as well.
	if(texture->handle->format == 0)
		if(!roRDriverCurrentContext->driver->initTexture(texture->handle, width, height, 1, pixelDataFormat, roRDriverTextureFlag_None))
			roEXCP_THROW;

	texture->width = width;
	texture->height = height;

	if(roRDriverCurrentContext->driver->updateTexture(texture->handle, 0, 0, pixelData.bytePtr(), 0, NULL)) {
		texture->state = Resource::Loaded;
		delete this;
	}
	else
		roEXCP_THROW;

roEXCP_CATCH
	nextFun = &PngLoader::abort;
	return reSchedule(false, taskPool->mainThreadId());
roEXCP_END
}

void PngLoader::abort(TaskPool* taskPool)
{
	texture->state = Resource::Aborted;
	delete this;
}

Resource* resourceCreatePng(ResourceManager* mgr, const char* uri)
{
	return new Texture(uri);
}

bool resourceLoadPng(ResourceManager* mgr, Resource* resource)
{
	Texture* texture = dynamic_cast<Texture*>(resource);
	if(!texture)
		return false;

	if(!texture->handle)
		texture->handle = roRDriverCurrentContext->driver->newTexture();

	TaskPool* taskPool = mgr->taskPool;
	PngLoader* loaderTask = new PngLoader(texture, mgr);
	texture->taskReady = texture->taskLoaded = taskPool->addFinalized(loaderTask, 0, 0, ~taskPool->mainThreadId());

	return true;
}

bool extMappingPng(const char* uri, void*& createFunc, void*& loadFunc)
{
	if(!uriExtensionMatch(uri, ".png"))
		return false;

	createFunc = resourceCreatePng;
	loadFunc = resourceLoadPng;
	return true;
}

}	// namespace ro
