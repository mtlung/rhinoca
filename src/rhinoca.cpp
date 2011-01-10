#include "pch.h"
#include "rhinoca.h"
#include "context.h"
#include "renderer/renderer.h"
#include <stdarg.h>	// For va_list

// Context
JSRuntime* jsrt = NULL;

void rhinoca_init()
{
	jsrt = JS_NewRuntime(8L * 1024L * 1024L);
	VERIFY(Render::init());
}

void rhinoca_close()
{
	JS_DestroyRuntime(jsrt);
	jsrt = NULL;
	Render::close();
}

Rhinoca* rhinoca_create(RhinocaRenderContext* renderContex)
{
	Rhinoca* context = new Rhinoca;
	return context;
}

void rhinoca_destroy(Rhinoca* context)
{
	delete context;
}

void rhinoca_setPrivate(Rhinoca* rh, void* data)
{
	rh->privateData = data;
}

void* rhinoca_getPrivate(Rhinoca* rh)
{
	return rh->privateData;
}

// Document
void rhinoca_openDocument(Rhinoca* rh, const char* url)
{
	if(!rh->openDoucment(url))
		print(rh, "Fail to load '%s'\n", url);
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

void rhinoca_processevent(Rhinoca* context, RhinocaEvent ev)
{
}

// IO
rhinoca_io_open io_open;
rhinoca_io_read io_read;
rhinoca_io_close io_close;

void rhinoca_io_setcallback(rhinoca_io_open open, rhinoca_io_read read, rhinoca_io_close close)
{
	io_open = open;
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

void rhinoca_collectGarbage(Rhinoca* rh)
{
	rh->collectGarbage();
}
