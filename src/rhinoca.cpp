#include "pch.h"
#include "rhinoca.h"
#include "assert.h"
#include "context.h"
#include "httpstream.h"
#include "../roar/base/roLog.h"
#include "socket.h"
#include "audio/audiodevice.h"
#include "render/driver.h"
#include "render/vg/openvg.h"
#include <stdarg.h>	// For va_list
#include <string.h>
#include <sys/stat.h>

using namespace ro;

// Context
JSRuntime* jsrt = NULL;
void* driverContext = NULL;

void rhinoca_init()
{
	jsrt = JS_NewRuntime(8L * 1024L * 1024L);
	Render::Driver::init();
	driverContext = Render::Driver::createContext(0);
	Render::Driver::useContext(driverContext);
	Render::Driver::forceApplyCurrent();

	RHVERIFY(vgCreateContextSH(1, 1));

	RHVERIFY(BsdSocket::initApplication() == 0);

	audiodevice_init();
}

void rhinoca_close()
{
	JS_DestroyRuntime(jsrt);
	JS_ShutDown();

	jsrt = NULL;
	vgDestroyContextSH();
	Render::Driver::deleteContext(driverContext);

	RHVERIFY(BsdSocket::closeApplication() == 0);

	audiodevice_close();
}

Rhinoca* rhinoca_create(RhinocaRenderContext* renderContext)
{
	Rhinoca* context = new Rhinoca(renderContext);
	return context;
}

void rhinoca_destroy(Rhinoca* context)
{
	delete context;
}

void* rhinoca_getPrivate(Rhinoca* rh)
{
	return rh->privateData;
}

void rhinoca_setPrivate(Rhinoca* rh, void* data)
{
	rh->privateData = data;
}

void rhinoca_setSize(Rhinoca* rh, rhuint width, rhuint height)
{
	if(width != 0 && height != 0) {
		rh->width = width;
		rh->height = height;
	}
}

rhuint rhinoca_getWidth(Rhinoca* rh)
{
	return rh->width;
}

rhuint rhinoca_getHeight(Rhinoca* rh)
{
	return rh->height;
}

Rhinoca* currentContext = NULL;

// Document
void rhinoca_openDocument(Rhinoca* rh, const char* uri)
{
	if(!rh->openDoucment(uri))
		roLog("error", "Fail to load '%s'\n", uri);
}

void rhinoca_closeDocument(Rhinoca* rh)
{
	rh->closeDocument();
}

// Main loop
void rhinoca_update(Rhinoca* context)
{
	context->update();
}

void rhinoca_processEvent(Rhinoca* context, RhinocaEvent ev)
{
	context->processEvent(ev);
}

// IO

struct CompoundFS
{
	CompoundFS() : type(None), handle(NULL) {}
	enum Type {
		None,
		Local,
		Http
	} type;
	void* handle;
};

static void* default_ioOpenFile(Rhinoca* rh, const char* uri)
{
	if(!uri || uri[0] == '\0')
		return NULL;

	CompoundFS* fs = new CompoundFS;
	if(strstr(uri, "http://") == uri) {
		fs->type = CompoundFS::Http;
		fs->handle = rhinoca_http_open(rh, uri);
	}
	else {
#if defined(RHINOCA_IOS)
		NSString* fullPath = [[NSBundle mainBundle] pathForResource:[NSString stringWithUTF8String:uri] ofType:nil];
		if(!fullPath) return NULL;
		
		fs->type = CompoundFS::Local;
		fs->handle = fopen([fullPath UTF8String], "rb");
#else
		fs->type = CompoundFS::Local;
		fs->handle = fopen(uri, "rb");
#endif
	}

	if(!fs->handle) {
		delete fs;
		fs = NULL;
	}

	return fs;
}

static bool default_ioReadReady(void* file, rhuint64 size)
{
	CompoundFS* fs = reinterpret_cast<CompoundFS*>(file);

	if(fs->type == CompoundFS::Http)
		return rhinoca_http_ready(fs->handle, size);

	return true;
}

static rhuint64 default_ioRead(void* file, void* buffer, rhuint64 size)
{
	CompoundFS* fs = reinterpret_cast<CompoundFS*>(file);

	if(fs->type == CompoundFS::Http)
		return rhinoca_http_read(fs->handle, buffer, size);

	FILE* f = reinterpret_cast<FILE*>(fs->handle);
	return (rhuint64)fread(buffer, 1, (size_t)size, f);
}

static rhint64 default_ioSize(void* file)
{
	CompoundFS* fs = reinterpret_cast<CompoundFS*>(file);

	if(fs->type == CompoundFS::Http)
		return -1;	// Currently http stream don't support getting file size

	FILE* f = reinterpret_cast<FILE*>(fs->handle);

	struct stat st;
	if(fstat(fileno(f), &st) != 0)
		return -1;

	return st.st_size;
}

static int default_ioSeek(void* file, rhuint64 offset, int origin)
{
	CompoundFS* fs = reinterpret_cast<CompoundFS*>(file);

	if(fs->type == CompoundFS::Http)
		return -1;	// Currently http stream don't support seek

	FILE* f = reinterpret_cast<FILE*>(fs->handle);
	return fseek(f, (long int)offset, origin) == 0 ? 1 : 0;
}

static void default_ioCloseFile(void* file)
{
	CompoundFS* fs = reinterpret_cast<CompoundFS*>(file);

	if(fs->type == CompoundFS::Http)
		rhinoca_http_close(fs->handle);
	else {
		FILE* f = reinterpret_cast<FILE*>(fs->handle);
		fclose(f);
	}

	delete fs;
}

struct OpenDirContext
{
#ifdef RHINOCA_VC
	~OpenDirContext() {
		if(handle != INVALID_HANDLE_VALUE)
			::FindClose(handle);
	}

	HANDLE handle;
	WIN32_FIND_DATAW data;
#endif

	TinyArray<char, 128> str;
};	// OpenDirContext

static bool toUtf16(const char* str, TinyArray<rhuint16, 128>& result)
{
	unsigned len = 0;
	if(!str || !roUtf8ToUtf16(NULL, len, str, UINT_MAX))
		return false;

	result.resize(len + 1);
	if(len == 0 || !roUtf8ToUtf16(&result[0], len, str, UINT_MAX))
		return false;

	result[len] = 0;

	return true;
}

static bool toUtf8(const rhuint16* str, TinyArray<char, 128>& result)
{
	unsigned len = 0;
	if(!str || !roUtf16ToUtf8(NULL, len, str, UINT_MAX))
		return false;

	result.resize(len + 1);
	if(len == 0 || !roUtf16ToUtf8(&result[0], len, str, UINT_MAX))
		return false;

	result[len] = 0;

	return true;
}

static void* default_ioOpenDir(Rhinoca* rh, const char* uri)
{
	if(!uri || uri[0] == '\0')
		return NULL;

	CompoundFS* fs = new CompoundFS;

	if(strstr(uri, "http://") == uri) {
	}
#ifdef RHINOCA_VC
	else {
		TinyArray<rhuint16, 128> buffer;
		if(!toUtf16(uri, buffer)) {
			delete fs;
			return NULL;
		}

		OpenDirContext* c = new OpenDirContext;

		// Add wild-card at the end of the path
		buffer.resize(buffer.size() - 1);	// Remove the null terminator first
		if(buffer.back() != L'/' && buffer.back() != L'\\')
			buffer.pushBack(L'/');
		buffer.pushBack(L'*');
		buffer.pushBack(L'\0');

		HANDLE h = ::FindFirstFileW((wchar_t*)&buffer[0], &(c->data));

		// Skip the ./ and ../
		while(::wcscmp(c->data.cFileName, L".") == 0 || ::wcscmp(c->data.cFileName, L"..") == 0) {
			if(!FindNextFileW(h, &(c->data))) {
				c->data.cFileName[0] = L'\0';
				break;
			}
		}

		if(h != INVALID_HANDLE_VALUE) {
			c->handle = h;
			fs->handle = c;
		}
		else
			delete c;
	}
#endif

	if(!fs->handle) {
		delete fs;
		fs = NULL;
	}

	return fs;
}

static bool default_ioNextDir(void* dir)
{
	if(!dir) return false;
	CompoundFS* fs = reinterpret_cast<CompoundFS*>(dir);

	if(fs->type == CompoundFS::Http) {
		return false;
	}
#ifdef RHINOCA_VC
	else {
		OpenDirContext* c = reinterpret_cast<OpenDirContext*>(fs->handle);

		if(!::FindNextFileW(c->handle, &(c->data))) {
			c->str.resize(1);
			c->str[0] = '\0';
			return false;
		}

		return true;
	}
#endif

	return false;
}

static const char* default_ioDirName(void* dir)
{
	if(!dir) return false;
	CompoundFS* fs = reinterpret_cast<CompoundFS*>(dir);

	if(fs->type == CompoundFS::Http) {
		return "";
	}
#ifdef RHINOCA_VC
	else {
		OpenDirContext* c = reinterpret_cast<OpenDirContext*>(fs->handle);

		if(!toUtf8((rhuint16*)c->data.cFileName, c->str)) {
			c->str.resize(1);
			c->str[0] = '\0';
		}

		return &c->str[0];
	}
#endif

	return "";
}

static void default_ioCloseDir(void* dir)
{
	if(!dir) return;
	CompoundFS* fs = reinterpret_cast<CompoundFS*>(dir);

	if(fs->type == CompoundFS::Http) {
	}
	else {
		OpenDirContext* c = reinterpret_cast<OpenDirContext*>(fs->handle);
		delete c;
	}

	delete fs;
}

RhFileSystem rhFileSystem = 
{
	default_ioOpenFile,
	default_ioReadReady,
	default_ioRead,
	default_ioSize,
	default_ioSeek,
	default_ioCloseFile,
	default_ioOpenDir,
	default_ioNextDir,
	default_ioDirName,
	default_ioCloseDir
};

RhFileSystem* rhinoca_getFileSystem()
{
	return &rhFileSystem;
}

void rhinoca_setFileSystem(RhFileSystem fs)
{
	rhFileSystem = fs;
}

// Memory allocation
void* rhinoca_realloca(void* ptr, unsigned int oldSize, unsigned int size);

// Others
rhinoca_alertFunc alertFunc = NULL;
void* alertFuncUserData = NULL;

void rhinoca_setAlertFunc(rhinoca_alertFunc alertFunc_, void* userData)
{
	alertFunc = alertFunc_;
	alertFuncUserData = userData;
}

void rhinoca_collectGarbage(Rhinoca* rh)
{
	rh->collectGarbage();
}

void rhinoca_setUserAgent(Rhinoca* rh, const char* userAgent)
{
	rh->userAgent = userAgent;
}
