#ifndef __CONTEXT_H__
#define __CONTEXT_H__

#include "common.h"
#include "resource.h"
#include "rhstring.h"
#include "dom/window.h"

extern rhinoca_io_open io_open;
extern rhinoca_io_read io_read;
extern rhinoca_io_close io_close;
extern rhinoca_printFunc print;

extern JSRuntime* jsrt;

struct Rhinoca
{
public:
	Rhinoca();
	~Rhinoca();

// Operations
	bool openDoucment(const char* uri);
	void closeDocument();

	void update();

	void collectGarbage();

// Attributes
	JSContext* jsContext;
	JSObject* jsGlobal;
	JSObject* jsConsole;
	void* privateData;
	FixString documentUrl;

	Dom::Window* domWindow;

	TaskPool taskPool;
	ResourceManager resourceManager;
};	// Rhinoca

#endif	// __CONTEXT_H__
