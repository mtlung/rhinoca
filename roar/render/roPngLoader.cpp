#include "pch.h"
#include "roTexture.h"
#include "roRenderDriver.h"
#include "../base/roFileSystem.h"
#include "../base/roLog.h"
#include "../base/roMemory.h"
#include "../base/roTypeCast.h"
#include "../../thirdParty/png/png.h"

#if roCOMPILER_VC
#	pragma comment(lib, "png")
#	pragma comment(lib, "zlib")
#endif

// Reading of png files using the libpng
// The code is base on the example provided in libpng
// More information can be found in
// http://www.libpng.org/
// http://www.libpng.org/pub/png/libpng-1.2.5-manual.html

namespace ro {

enum TextureLoadingState {
	TextureLoadingState_LoadHeader,
	TextureLoadingState_InitTexture,
	TextureLoadingState_LoadPixelData,
	TextureLoadingState_Commit,
	TextureLoadingState_Finish,
	TextureLoadingState_Abort
};

Resource* resourceCreatePng(const char* uri, ResourceManager* mgr)
{
	if(uriExtensionMatch(uri, ".png"))
		return new Texture(uri);
	return NULL;
}

class PngLoader : public Task
{
public:
	PngLoader(Texture* t, ResourceManager* mgr);

	~PngLoader()
	{
		if(stream) fileSystem.closeFile(stream);
		roFree(pixelData);
		// libpng will handle for null input pointers
		png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
	}

	override void run(TaskPool* taskPool);

	void loadHeader();
	void initTexture(TaskPool* taskPool);
	void loadPixelData();
	void commit(TaskPool* taskPool);

	void* stream;
	Texture* texture;
	ResourceManager* manager;
	unsigned width, height;
	roBytePtr pixelData;
	roSize pixelDataSize;
	roSize rowBytes;
	roRDriverTextureFormat pixelDataFormat;

	png_infop info_ptr;
	png_structp png_ptr;
	int currentPass;		// For keep tracking when a new pass is loaded

	TextureLoadingState textureLoadingState;
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
	roAssert(impl->pixelData);

	// Have libpng either combine the new row data with the existing row data
	// from previous passes (if interlaced) or else just copy the new row
	// into the main program's image buffer
	png_progressive_combine_row(png_ptr, (impl->pixelData + row_num * impl->rowBytes).cast<png_byte>(), new_row);

	// Only change the loading state after a pass is finished
	if(pass > impl->currentPass) {
		impl->currentPass = pass;
		// TODO: Implement progressive loading
		//impl->mLoadingState = PartialLoaded;
	}
}

static void end_callback(png_structp png_ptr, png_infop)
{
	PngLoader* impl = reinterpret_cast<PngLoader*>(png_get_progressive_ptr(png_ptr));
	impl->loadPixelData();
}

PngLoader::PngLoader(Texture* t, ResourceManager* mgr)
	: stream(NULL), texture(t), manager(mgr)
	, width(0), height(0)
	, pixelData(NULL), pixelDataSize(0), rowBytes(0), pixelDataFormat(roRDriverTextureFormat_RGBA)
	, info_ptr(NULL), png_ptr(NULL)
	, currentPass(0)
	, textureLoadingState(TextureLoadingState_LoadHeader)
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
	char buff[1024*8];
	roUint64 bytesRead = 0;

	if(textureLoadingState == TextureLoadingState_InitTexture)
		initTexture(taskPool);
	else if(textureLoadingState == TextureLoadingState_Commit)
		commit(taskPool);
	else if(setjmp(png_jmpbuf(png_ptr)))
		textureLoadingState = TextureLoadingState_Abort;
	else do
	{
		Status st;
		if(!stream) st = fileSystem.openFile(texture->uri(), stream);
		if(!st) {
			roLog("error", "PngLoader: Fail to open file '%s', reason: %s\n", texture->uri().c_str(), st.c_str());
			textureLoadingState = TextureLoadingState_Abort;
			break;
		}

		if(fileSystem.readWillBlock(stream, sizeof(buff))) {
			// Re-schedule the load operation
			return reSchedule();
		}

		st = fileSystem.read(stream, buff, sizeof(buff), bytesRead);
		if(!st) {
			textureLoadingState = TextureLoadingState_Abort;
			break;
		}
		png_process_data(png_ptr, info_ptr, (png_bytep)buff, num_cast<png_size_t>(bytesRead));
	} while(bytesRead > 0 && textureLoadingState != TextureLoadingState_Abort);

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

void PngLoader::loadHeader()
{
	// Query the info structure.
	int bit_depth, color_type, interlace_method;
	png_get_IHDR(png_ptr, info_ptr,
		(png_uint_32*)(&width), (png_uint_32*)(&height),
		&bit_depth, &color_type, &interlace_method, NULL, NULL);

	switch(color_type) {
	case PNG_COLOR_TYPE_RGB:
//		pixelDataFormat = Driver::RGB;
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
		pixelDataFormat = roRDriverTextureFormat_RGBA;	// NOTE: I am not quite sure to use RGB or RGBA
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

	roAssert(!pixelData);
	pixelDataSize = rowBytes * height;
	pixelData = roMalloc(pixelDataSize);

	textureLoadingState = TextureLoadingState_InitTexture;
	return reSchedule(false, manager->taskPool->mainThreadId());

Abort:
	textureLoadingState = TextureLoadingState_Abort;
}

void PngLoader::initTexture(TaskPool* taskPool)
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

void PngLoader::loadPixelData()
{
	textureLoadingState = TextureLoadingState_Commit;
	return reSchedule(false, manager->taskPool->mainThreadId());
}

void PngLoader::commit(TaskPool* taskPool)
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

bool resourceLoadPng(Resource* resource, ResourceManager* mgr)
{
	if(!uriExtensionMatch(resource->uri(), ".png")) return false;

	TaskPool* taskPool = mgr->taskPool;

	Texture* texture = dynamic_cast<Texture*>(resource);
	texture->handle = roRDriverCurrentContext->driver->newTexture();

	PngLoader* loaderTask = new PngLoader(texture, mgr);
	texture->taskReady = texture->taskLoaded = taskPool->addFinalized(loaderTask, 0, 0, ~taskPool->mainThreadId());

	return true;
}

}	// namespace ro
