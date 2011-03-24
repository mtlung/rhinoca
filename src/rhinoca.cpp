#include "pch.h"
#include "rhinoca.h"
#include "context.h"
#include "socket.h"
#include "render/driver.h"
#include "render/vg/openvg.h"
#include <stdarg.h>	// For va_list

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
}

void rhinoca_close()
{
	JS_DestroyRuntime(jsrt);
	JS_ShutDown();

	jsrt = NULL;
	vgDestroyContextSH();
	Render::Driver::deleteContext(driverContext);

	VERIFY(BsdSocket::closeApplication() == 0);
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
rhinoca_io_open io_open;
rhinoca_io_ready io_ready;
rhinoca_io_read io_read;
rhinoca_io_close io_close;

void rhinoca_io_setcallback(rhinoca_io_open open, rhinoca_io_ready ready, rhinoca_io_read read, rhinoca_io_close close)
{
	io_open = open;
	io_ready = ready;
	io_read = read;
	io_close = close;
}

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
