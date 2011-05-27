#ifndef __CONTEXT_H__
#define __CONTEXT_H__

#include "common.h"
#include "resource.h"
#include "rhstring.h"
#include "dom/window.h"

extern JSRuntime* jsrt;

struct AudioDevice;

struct Rhinoca
{
public:
	explicit Rhinoca(RhinocaRenderContext* rc);
	~Rhinoca();

// Operations
	bool openDoucment(const char* uri);
	void closeDocument();

	void update();

	void processEvent(RhinocaEvent ev);

	void collectGarbage();

	void _initGlobal();

// Attributes
	JSContext* jsContext;
	JSObject* jsGlobal;
	JSObject* jsConsole;
	void* privateData;
	FixString documentUrl;

	Dom::Window* domWindow;

	unsigned width, height;

	RhinocaRenderContext* renderContex;

	AudioDevice* audioDevice;

	TaskPool taskPool;
	ResourceManager resourceManager;
};	// Rhinoca

#endif	// __CONTEXT_H__
