#include "pch.h"
#include "rhinoca.h"
#include "assert.h"
#include "context.h"
#include "../roar/base/roHttpFileSystem.h"
#include "../roar/base/roLog.h"
#include "../roar/base/roSocket.h"
#include "../roar/render/roRenderDriver.h"
#include "audio/audiodevice.h"
#include <stdarg.h>	// For va_list
#include <string.h>
#include <sys/stat.h>

//////////////////////////////////////////////////////////////////////////
// A note on how web browser works:
// http://taligarsiel.com/Projects/howbrowserswork1.htm

using namespace ro;

// Context
JSRuntime* jsrt = NULL;

void rhinoca_init()
{
	jsrt = JS_NewRuntime(8L * 1024L * 1024L);

	roVerify(BsdSocket::initApplication() == 0);

	audiodevice_init();
}

void rhinoca_close()
{
	JS_DestroyRuntime(jsrt);
	JS_ShutDown();

	jsrt = NULL;
//	vgDestroyContextSH();

	roVerify(BsdSocket::closeApplication() == 0);

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
		rh->screenResized();
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
	context->subSystems.renderDriver->swapBuffers();
}

void rhinoca_processEvent(Rhinoca* context, RhinocaEvent ev)
{
	context->processEvent(ev);
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
