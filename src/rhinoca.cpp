#include "pch.h"
#include "rhinoca.h"
#include "context.h"
#include "socket.h"
#include "audio/audiodevice.h"
#include "render/driver.h"
#include "render/vg/openvg.h"
#include <stdarg.h>	// For va_list
#include <string.h>
#include <sys/stat.h>

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

	VERIFY(vgCreateContextSH(1, 1));

	VERIFY(BsdSocket::initApplication() == 0);

	audiodevice_init();
}

void rhinoca_close()
{
	JS_DestroyRuntime(jsrt);
	JS_ShutDown();

	jsrt = NULL;
	vgDestroyContextSH();
	Render::Driver::deleteContext(driverContext);

	VERIFY(BsdSocket::closeApplication() == 0);

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
		print(rh, "Fail to load '%s'\n", uri);
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
	enum Type {
		Local,
		Http
	} type;
	void* handle;
};

static void* default_ioOpen(Rhinoca* rh, const char* uri, int threadId)
{
	if(uri[0] == '\0')
		return NULL;

	CompoundFS* fs = new CompoundFS;
	if(strstr(uri, "http://") == uri) {
		fs->type = CompoundFS::Http;
		fs->handle = rhinoca_http_open(rh, uri, threadId);
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

static bool default_ioReady(void* file, rhuint64 size, int threadId)
{
	CompoundFS* fs = reinterpret_cast<CompoundFS*>(file);

	if(fs->type == CompoundFS::Http)
		return rhinoca_http_ready(fs->handle, size, threadId);

	return true;
}

static rhuint64 default_ioRead(void* file, void* buffer, rhuint64 size, int threadId)
{
	CompoundFS* fs = reinterpret_cast<CompoundFS*>(file);

	if(fs->type == CompoundFS::Http)
		return rhinoca_http_read(fs->handle, buffer, size, threadId);

	FILE* f = reinterpret_cast<FILE*>(fs->handle);
	return (rhuint64)fread(buffer, 1, (size_t)size, f);
}

static rhint64 default_ioSize(void* file, int threadId)
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

static int default_ioSeek(void* file, rhuint64 offset, int origin, int threadId)
{
	CompoundFS* fs = reinterpret_cast<CompoundFS*>(file);

	if(fs->type == CompoundFS::Http)
		return -1;	// Currently http stream don't support seek

	FILE* f = reinterpret_cast<FILE*>(fs->handle);
	return fseek(f, (long int)offset, origin) == 0 ? 1 : 0;
}

static void default_ioClose(void* file, int threadId)
{
	CompoundFS* fs = reinterpret_cast<CompoundFS*>(file);

	if(fs->type == CompoundFS::Http)
		rhinoca_http_close(fs->handle, threadId);
	else {
		FILE* f = reinterpret_cast<FILE*>(fs->handle);
		fclose(f);
	}

	delete fs;
}

rhinoca_io_open io_open = default_ioOpen;
rhinoca_io_ready io_ready = default_ioReady;
rhinoca_io_read io_read = default_ioRead;
rhinoca_io_size io_size = default_ioSize;
rhinoca_io_seek io_seek = default_ioSeek;
rhinoca_io_close io_close = default_ioClose;

rhinoca_io_open rhinoca_get_io_open()	{ return io_open; }
rhinoca_io_ready rhinoca_get_io_ready()	{ return io_ready; }
rhinoca_io_read rhinoca_get_io_read()	{ return io_read; }
rhinoca_io_seek rhinoca_get_io_seek()	{ return io_seek; }
rhinoca_io_close rhinoca_get_io_close()	{ return io_close; }

void rhinoca_set_io_open(rhinoca_io_open f)		{ io_open = f; }
void rhinoca_set_io_ready(rhinoca_io_ready f)	{ io_ready = f; }
void rhinoca_set_io_read(rhinoca_io_read f)		{ io_read = f; }
void rhinoca_set_io_size(rhinoca_io_size f)		{ io_size = f; }
void rhinoca_set_io_seek(rhinoca_io_seek f)		{ io_seek = f; }
void rhinoca_set_io_close(rhinoca_io_close f)	{ io_close = f; }

// Memory allocation
void* rhinoca_realloca(void* ptr, unsigned int oldSize, unsigned int size);

// Others
static void defaultPrintFunc(Rhinoca* context, const char* str, ...)
{
	if(!context || !str) return;

	va_list vl;
	va_start(vl, str);
	vprintf(str, vl);
	va_end(vl);
}

rhinoca_printFunc print = defaultPrintFunc;

void rhinoca_setPrintFunc(rhinoca_printFunc printFunc)
{
	print = printFunc;
}

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
