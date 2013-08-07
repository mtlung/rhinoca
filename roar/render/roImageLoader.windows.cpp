#include "pch.h"
#include "roTexture.h"
#include "roRenderDriver.h"
#include "../base/roComPtr.h"
#include "../base/roCpuProfiler.h"
#include "../base/roFileSystem.h"
#include "../base/roLog.h"
#include "../base/roMemory.h"
#include "../base/roTypeCast.h"
#include "../platform/roPlatformHeaders.h"

#include <wincodec.h>

#pragma comment(lib, "Windowscodecs.lib")

// http://www.spacesimulator.net/tut4_3dsloader.html
// http://www.gamedev.net/reference/articles/article1966.asp
// http://msdn.microsoft.com/en-us/magazine/cc500647.aspx

// From Microsoft SDK 7.0a
//static const GUID FAR GUID_WICPixelFormat32bppRGBA = { 0xf5c7ad2d, 0x6a8d, 0x43dd, 0xa7, 0xa8, 0xa2, 0x99, 0x35, 0x26, 0x1a, 0xe9 };

namespace ro {

namespace {

struct InitCom
{
	InitCom() { ::CoInitialize(NULL); }
	~InitCom() { ::CoUninitialize(); }
};

static InitCom _initCom;

struct WICConvert
{
	GUID source;
	GUID target;
	roRDriverTextureFormat driverFormat;
};

// Formats that were not listed here will by default convert to roRDriverTextureFormat_RGBA with a warning log
static const WICConvert _wicConvert[] = 
{
	{ GUID_WICPixelFormatBlackWhite,			GUID_WICPixelFormat8bppGray,		roRDriverTextureFormat_L		},
	{ GUID_WICPixelFormat2bppGray,				GUID_WICPixelFormat8bppGray,		roRDriverTextureFormat_L		},
	{ GUID_WICPixelFormat4bppGray,				GUID_WICPixelFormat8bppGray,		roRDriverTextureFormat_L		},
    { GUID_WICPixelFormat8bppGray,				GUID_WICPixelFormat8bppGray,		roRDriverTextureFormat_L		},
	{ GUID_WICPixelFormat16bppGrayFixedPoint,	GUID_WICPixelFormat8bppGray,		roRDriverTextureFormat_L		},
	{ GUID_WICPixelFormat32bppGrayFixedPoint,	GUID_WICPixelFormat8bppGray,		roRDriverTextureFormat_L		},

	{ GUID_WICPixelFormat24bppBGR,				GUID_WICPixelFormat32bppRGBA,		roRDriverTextureFormat_RGBA		},
	{ GUID_WICPixelFormat24bppRGB,				GUID_WICPixelFormat32bppRGBA,		roRDriverTextureFormat_RGBA		},
    { GUID_WICPixelFormat32bppRGBA,				GUID_WICPixelFormat32bppRGBA,		roRDriverTextureFormat_RGBA		},
	{ GUID_WICPixelFormat32bppBGRA,				GUID_WICPixelFormat32bppRGBA,		roRDriverTextureFormat_RGBA		},
	{ GUID_WICPixelFormat32bppPBGRA,			GUID_WICPixelFormat32bppRGBA,		roRDriverTextureFormat_RGBA		},
//	{ GUID_WICPixelFormat32bppPRGBA,			GUID_WICPixelFormat32bppRGBA,		roRDriverTextureFormat_RGBA		},

	{ GUID_WICPixelFormat1bppIndexed,			GUID_WICPixelFormat32bppRGBA,		roRDriverTextureFormat_RGBA		},
	{ GUID_WICPixelFormat2bppIndexed,			GUID_WICPixelFormat32bppRGBA,		roRDriverTextureFormat_RGBA		},
	{ GUID_WICPixelFormat4bppIndexed,			GUID_WICPixelFormat32bppRGBA,		roRDriverTextureFormat_RGBA		},
	{ GUID_WICPixelFormat8bppIndexed,			GUID_WICPixelFormat32bppRGBA,		roRDriverTextureFormat_RGBA		},

	{ GUID_WICPixelFormat48bppRGBFixedPoint,	GUID_WICPixelFormat64bppRGBAHalf,	roRDriverTextureFormat_RGBA_16F	},
//	{ GUID_WICPixelFormat48bppBGRFixedPoint,	GUID_WICPixelFormat64bppRGBAHalf,	roRDriverTextureFormat_RGBA_16F },
	{ GUID_WICPixelFormat64bppRGBAFixedPoint,	GUID_WICPixelFormat64bppRGBAHalf,	roRDriverTextureFormat_RGBA_16F },
//	{ GUID_WICPixelFormat64bppBGRAFixedPoint,	GUID_WICPixelFormat64bppRGBAHalf,	roRDriverTextureFormat_RGBA_16F },
	{ GUID_WICPixelFormat64bppRGBFixedPoint,	GUID_WICPixelFormat64bppRGBAHalf,	roRDriverTextureFormat_RGBA_16F },
	{ GUID_WICPixelFormat64bppRGBHalf,			GUID_WICPixelFormat64bppRGBAHalf,	roRDriverTextureFormat_RGBA_16F },
	{ GUID_WICPixelFormat48bppRGBHalf,			GUID_WICPixelFormat64bppRGBAHalf,	roRDriverTextureFormat_RGBA_16F },
    { GUID_WICPixelFormat64bppRGBAHalf,			GUID_WICPixelFormat64bppRGBAHalf,	roRDriverTextureFormat_RGBA_16F	},
    { GUID_WICPixelFormat48bppRGB,				GUID_WICPixelFormat64bppRGBAHalf,	roRDriverTextureFormat_RGBA_16F },
    { GUID_WICPixelFormat64bppRGBA,				GUID_WICPixelFormat64bppRGBAHalf,	roRDriverTextureFormat_RGBA_16F	},

    { GUID_WICPixelFormat128bppPRGBAFloat,		GUID_WICPixelFormat128bppRGBAFloat,	roRDriverTextureFormat_RGBA_32F	},
    { GUID_WICPixelFormat128bppRGBAFloat,		GUID_WICPixelFormat128bppRGBAFloat,	roRDriverTextureFormat_RGBA_32F	},
	{ GUID_WICPixelFormat128bppRGBFloat,		GUID_WICPixelFormat128bppRGBAFloat,	roRDriverTextureFormat_RGBA_32F	},
	{ GUID_WICPixelFormat128bppRGBAFixedPoint,	GUID_WICPixelFormat128bppRGBAFloat,	roRDriverTextureFormat_RGBA_32F	},
	{ GUID_WICPixelFormat128bppRGBFixedPoint,	GUID_WICPixelFormat128bppRGBAFloat,	roRDriverTextureFormat_RGBA_32F	},

	{ GUID_WICPixelFormat32bppCMYK,				GUID_WICPixelFormat32bppRGBA,		roRDriverTextureFormat_RGBA	}
};

static const WICConvert _defaultWicConvert = { GUID(), GUID_WICPixelFormat32bppRGBA, roRDriverTextureFormat_RGBA };

static const WICConvert& _getConvertFormat(GUID source)
{
	for(roSize i=0; i<roCountof(_wicConvert); ++i) {
		if(_wicConvert[i].source == source)
			return _wicConvert[i];
	}

	roLog("warn", "WICLoader: Fail to find exact matched texture format, a lower precision format is used.\n");
	return _defaultWicConvert;
}

}	// namespace

class WICLoader : public Task
{
public:
	WICLoader(Texture* t, ResourceManager* mgr)
		: stream(NULL), texture(t), manager(mgr)
		, width(0), height(0)
		, pixelData(NULL)
		, rowStride(0)
		, nextFun(&WICLoader::initWic)
	{}

	~WICLoader()
	{
		if(stream) fileSystem.closeFile(stream);
	}

	override void run(TaskPool* taskPool);

protected:
	void initWic(TaskPool* taskPool);
	void loadFileToMemory(TaskPool* taskPool);
	void decodeWic(TaskPool* taskPool);
	void initTexture(TaskPool* taskPool);
	void commit(TaskPool* taskPool);
	void abort(TaskPool* taskPool);

	void* stream;
	TexturePtr texture;
	ResourceManager* manager;
	UINT width, height;
	Array<roUint8> pixelData;

	CComPtr<IWICImagingFactory> wic;
	WICPixelFormatGUID pixelFormat;

	UINT bpp;
	UINT rowStride;
	roRDriverTextureFormat textureFormat;

	void (WICLoader::*nextFun)(TaskPool*);
};

void WICLoader::run(TaskPool* taskPool)
{
	CpuProfilerScope cpuProfilerScope(__FUNCTION__);

	if(texture->state == Resource::Aborted || !taskPool->keepRun()) {
		nextFun = &WICLoader::abort;

		if(taskPool->threadId() != taskPool->mainThreadId())
			return reSchedule(false, taskPool->mainThreadId());
	}

	(this->*nextFun)(taskPool);
}

void WICLoader::initWic(TaskPool* taskPool)
{
	HRESULT hr = ::CoCreateInstance(
		CLSID_WICImagingFactory, NULL,
		CLSCTX_INPROC_SERVER, __uuidof(IWICImagingFactory),
		(LPVOID*)&wic.p
	);

	if(FAILED(hr)) {
		roLog("error", "WindowsImagingLoader: Fail to create IWICImagingFactory\n");
		nextFun = &WICLoader::abort;
		return reSchedule(false, taskPool->mainThreadId());
	}

	nextFun = &WICLoader::loadFileToMemory;
	return reSchedule(false, ~taskPool->mainThreadId());
}

void WICLoader::loadFileToMemory(TaskPool* taskPool)
{
	Status st;

roEXCP_TRY
	if(!stream) st = fileSystem.openFile(texture->uri(), stream);
	if(!st) roEXCP_THROW;

	// If data not ready, give up in this round and do it again in next schedule
	if(fileSystem.readWillBlock(stream, roUint64(-1)))
		return reSchedule();

	nextFun = &WICLoader::decodeWic;
	return reSchedule(false, ~taskPool->mainThreadId());

roEXCP_CATCH
	roLog("error", "WICLoader: Fail to load '%s', reason: %s\n", texture->uri().c_str(), st.c_str());
	nextFun = &WICLoader::abort;
	return reSchedule(false, taskPool->mainThreadId());

roEXCP_END
}

static UINT _getBitsPerPixel(IWICImagingFactory* wic, REFGUID targetGuid)
{
	roAssert(wic);

	CComPtr<IWICComponentInfo> cinfo;
	if(FAILED(wic->CreateComponentInfo(targetGuid, &cinfo)))
		return 0;

	WICComponentType type;
	if(FAILED(cinfo->GetComponentType(&type)))
		return 0;

	if(type != WICPixelFormat)
		return 0;

	CComPtr<IWICPixelFormatInfo> pfinfo;
	if(FAILED(cinfo->QueryInterface( __uuidof(IWICPixelFormatInfo), reinterpret_cast<void**>(&pfinfo))))
		return 0;

	UINT bpp;
	if(FAILED(pfinfo->GetBitsPerPixel(&bpp)))
		return 0;

	return bpp;
}

void WICLoader::decodeWic(TaskPool* taskPool)
{
	Status st;

roEXCP_TRY
	if(!wic) roEXCP_THROW;

	roUint64 readableSize = 0;
	char* buf = fileSystem.getBuffer(stream, roUint64(-1), readableSize);

    if(!buf || !readableSize) {
        st = roStatus::file_is_empty;
        roEXCP_THROW;
    }

	DWORD wicSize = 0;
	st = roSafeAssign(wicSize, readableSize);
	if(!st) roEXCP_THROW;

	st = roStatus::undefined;
	CComPtr<IWICStream> wicStream;
	HRESULT hr = wic->CreateStream(&wicStream);
	if(FAILED(hr)) roEXCP_THROW;

	hr = wicStream->InitializeFromMemory((BYTE*)buf, wicSize);
	if(FAILED(hr)) roEXCP_THROW;

    // Look for WINCODEC_ERR_ in wincodec.h if something failed
	CComPtr<IWICBitmapDecoder> decoder = NULL;
	hr = wic->CreateDecoderFromStream(wicStream, 0, WICDecodeMetadataCacheOnDemand, &decoder);
	if(FAILED(hr)) roEXCP_THROW;

	CComPtr<IWICBitmapFrameDecode> frame = NULL;
	hr = decoder->GetFrame(0, &frame);
	if(FAILED(hr)) roEXCP_THROW;

	hr = frame->GetSize(&width, &height);
	if(FAILED(hr)) roEXCP_THROW;

	hr = frame->GetPixelFormat(&pixelFormat);
	if(FAILED(hr)) roEXCP_THROW;

	CComPtr<IWICFormatConverter> converter = NULL;
	hr = wic->CreateFormatConverter(&converter);
	if(FAILED(hr)) roEXCP_THROW;

	const WICConvert& convertFormat = _getConvertFormat(pixelFormat);
	GUID dstWicFormat = convertFormat.target;
	textureFormat = convertFormat.driverFormat;

	hr = converter->Initialize(frame, dstWicFormat, WICBitmapDitherTypeErrorDiffusion, 0, 0, WICBitmapPaletteTypeCustom);
	if(FAILED(hr)) roEXCP_THROW;

	bpp = _getBitsPerPixel(wic, dstWicFormat);
	rowStride = (width * bpp + 7) / 8;

	UINT imageSizeInBytes = rowStride * height;
	st = pixelData.resize(imageSizeInBytes);
	if(!st) roEXCP_THROW;

	hr = converter->CopyPixels(0, rowStride, imageSizeInBytes, pixelData.castedPtr<BYTE>());  
	if(FAILED(hr)) roEXCP_THROW;

	nextFun = &WICLoader::initTexture;

roEXCP_CATCH
	roLog("error", "WICLoader: Fail to load '%s', reason: %s\n", texture->uri().c_str(), st.c_str());
	nextFun = &WICLoader::abort;

roEXCP_END
	return reSchedule(false, taskPool->mainThreadId());
}

void WICLoader::initTexture(TaskPool* taskPool)
{
	if(roRDriverCurrentContext->driver->initTexture(texture->handle, width, height, 1, textureFormat, roRDriverTextureFlag_None))
		nextFun = &WICLoader::commit;
	else
		nextFun = &WICLoader::abort;

	return reSchedule(false, taskPool->mainThreadId());
}

void WICLoader::commit(TaskPool* taskPool)
{
	if(roRDriverCurrentContext->driver->updateTexture(texture->handle, 0, 0, pixelData.bytePtr(), 0, NULL)) {
		texture->state = Resource::Loaded;
		delete this;
	}
	else {
		nextFun = &WICLoader::abort;
		return reSchedule(false, taskPool->mainThreadId());
	}
}

void WICLoader::abort(TaskPool* taskPool)
{
	texture->state = Resource::Aborted;
	delete this;
}

Resource* resourceCreateImage(ResourceManager* mgr, const char* uri)
{
	return new Texture(uri);
}

bool resourceLoadImage(ResourceManager* mgr, Resource* resource)
{
	Texture* texture = dynamic_cast<Texture*>(resource);
	if(!texture)
		return false;

	if(!texture->handle)
		texture->handle = roRDriverCurrentContext->driver->newTexture();

	TaskPool* taskPool = mgr->taskPool;
	WICLoader* loaderTask = new WICLoader(texture, mgr);
	texture->taskReady = texture->taskLoaded = taskPool->addFinalized(loaderTask, 0, 0, taskPool->mainThreadId());

	return true;
}

bool extMappingImage(const char* uri, void*& createFunc, void*& loadFunc)
{
	static const char* supported[] = {
		".bmp", ".png", ".ico", ".jpg", ".jpeg", ".gif", ".tiff", ".hdp"
	};

	for(roSize i=0; i<roCountof(supported); ++i) {
		if(uriExtensionMatch(uri, supported[i])) {
			createFunc = resourceCreateImage;
			loadFunc = resourceLoadImage;
			return true;
		}
	}

	return false;
}

}	// namespace ro
