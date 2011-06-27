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
	FixString userAgent;
	FixString documentUrl;

	Dom::Window* domWindow;

	unsigned width, height;

	RhinocaRenderContext* renderContex;

	AudioDevice* audioDevice;

	TaskPool taskPool;
	ResourceManager resourceManager;

	/// Parameter to tune how often we perform explicit GC.
	/// It's kind of temporary solution, since I am seeking some multi-thready way to do GC
	unsigned _gcFrameIntervalCounter;
	static const unsigned _gcFrameInterval = 60;
};	// Rhinoca

#endif	// __CONTEXT_H__
