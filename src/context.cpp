#include "pch.h"
#include "context.h"
#include "common.h"
#include "path.h"
#include "platform.h"
#include "rhinoca.h"
#include "xmlparser.h"
#include "dom/body.h"
#include "dom/element.h"
#include "dom/event.h"
#include "dom/document.h"
#include "dom/keyevent.h"
#include "dom/mouseevent.h"
#include "dom/textnode.h"
#include "dom/touchevent.h"
#include "dom/registerfactories.h"
#include "../roar/base/roFileSystem.h"
#include "../roar/base/roLog.h"
#include "../roar/base/roTypeCast.h"
#include "../roar/audio/roAudioDriver.h"
#include "../roar/render/roRenderDriver.h"
#include "../roar/base/roCpuProfiler.h"

#ifdef RHINOCA_IOS
#	include "context_ios.h"
#else
#	include "context_win.h"
#endif

#ifdef RHINOCA_VC
#	ifndef NDEBUG
#		pragma comment(lib, "js32d")
#	else
#		pragma comment(lib, "js32")
#	endif
#endif

using namespace ro;

void jsReportError(JSContext* cx, const char* message, JSErrorReport* report)
{
	roLog("js", "JS %s: %s:%u\n%s\n",
		JSREPORT_IS_WARNING(report->flags) ? "warning" : "error",
		report->filename ? report->filename : "<no filename>",
		(unsigned int)report->lineno,
		message
	);
}

static JSClass jsConsoleClass = {
	"console", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
	JSCLASS_NO_OPTIONAL_MEMBERS
};

JSBool jsConsoleLog(JSContext* cx, uintN argc, jsval* vp)
{
	JsString jss(cx, JS_ARGV0);
	if(!jss) return JS_FALSE;

	roLog("js", "%s\n", jss.c_str());
	return JS_TRUE;
}

static JSFunctionSpec jsConsoleMethods[] = {
	{"log", jsConsoleLog, 1,0},
	{0}
};

struct RhinocaRenderContext
{
	void* platformSpecificContext;
	unsigned fbo;
	unsigned depth;
	unsigned stencil;
	unsigned texture;
};

static void _initRenderDriver(ro::SubSystems& subSystems)
{
	Rhinoca& rh = *(Rhinoca*)(subSystems.userData[0]);
	roRDriver* driver = roNewRenderDriver((char*)subSystems.userData[1], NULL);
	roRDriverContext* context = driver->newContext(driver);
	context->majorVersion = 2;
	context->minorVersion = 0;
	driver->initContext(context, rh.renderContex->platformSpecificContext);
	driver->useContext(context);
	driver->setViewport(0, 0, context->width, context->height, 0, 1);

	subSystems.renderDriver = driver;
	subSystems.renderContext = context;
}

Rhinoca::Rhinoca(RhinocaRenderContext* rc)
	: privateData(NULL)
	, domWindow(NULL)
	, width(0), height(0)
	, renderContex(rc)
	, _gcFrameIntervalCounter(_gcFrameInterval)
{
	jsContext = JS_NewContext(jsrt, 8192);
	JS_SetOptions(jsContext, JS_GetOptions(jsContext) | JSOPTION_JIT);
//	JS_SetOptions(jsContext, JS_GetOptions(jsContext) | JSOPTION_STRICT);
//	JS_SetOptions(jsContext, JS_GetOptions(jsContext) | JSOPTION_VAROBJFIX);
	JS_SetContextPrivate(jsContext, this);
	JS_SetErrorReporter(jsContext, jsReportError);

	subSystems.userData.pushBack(this);
	subSystems.userData.pushBack((void*)"GL");
	subSystems.initRenderDriver = _initRenderDriver;
	subSystems.init();

	Dom::registerFactories();

#ifdef RHINOCA_IOS
	userAgent = "Mozilla/5.0 (iPhone; U; CPU iPhone OS 3_0 like Mac OS X; en-us) AppleWebKit/528.18 (KHTML, like Gecko) Version/4.0 Mobile/7A341 Safari/528.16";
#else
	userAgent = "Mozilla/5.0 (Windows NT 6.0; WOW64; rv:5.0) Gecko/20100101 Firefox/5.0";
#endif
}

Rhinoca::~Rhinoca()
{
	closeDocument();

	JS_DestroyContext(jsContext);
}

void Rhinoca::update()
{
	subSystems.tick();

	CpuProfilerScope cpuProfilerScope(__FUNCTION__);

	if(domWindow)
		domWindow->update();

	for(Dom::Node::TickEntry* n = audioTickList.begin(); n != audioTickList.end(); )
	{
		Dom::Node::TickEntry* next = n->next();
		Dom::Node& node = *(roContainerof(Dom::Node, tickListNode, n));
		node.tick(0);
		n = next;
	}

	if(domWindow) {
		domWindow->render();

		if(--_gcFrameIntervalCounter == 0) {
			CpuProfilerScope cpuProfilerScope("JS_MaybeGC");
			_gcFrameIntervalCounter = _gcFrameInterval;
			JS_MaybeGC(jsContext);
		}
	}
}

void Rhinoca::screenResized()
{
	subSystems.renderDriver->changeResolution(width, height);
	subSystems.renderDriver->setViewport(0, 0, width, height, 0, 1);
	subSystems.currentCanvas = NULL;
}

void Rhinoca::collectGarbage()
{
	CpuProfilerScope cpuProfilerScope("JS_GC");
	JS_GC(jsContext);
}

void Rhinoca::_initGlobal()
{
	roAssert(!domWindow);

	// NOTE: We use the Window object as the global
	jsGlobal = JS_NewCompartmentAndGlobalObject(jsContext, &Dom::Window::jsClass, NULL);
	JS_SetGlobalObject(jsContext, jsGlobal);

	roVerify(JS_InitStandardClasses(jsContext, jsGlobal));

	Dom::registerClasses(jsContext, jsGlobal);

	domWindow = new Dom::Window(this);
	domWindow->jsObject = jsGlobal;
	domWindow->bind(jsContext, jsGlobal);
	domWindow->addGcRoot();	// releaseGcRoot() in ~Rhinoca()

	{	// Register the console object
		jsConsole = JS_DefineObject(jsContext, jsGlobal, "console", &jsConsoleClass, 0, JSPROP_ENUMERATE);
		roVerify(JS_SetPrivate(jsContext, jsConsole, this));
		roVerify(JS_AddNamedObjectRoot(jsContext, &jsConsole, "console"));
		roVerify(JS_DefineFunctions(jsContext, jsConsole, jsConsoleMethods));
	}

	{	// Run some default settings
		const char* script =
			"var window = this;"
			"function log(msg){setTimeout(function(){throw new Error(msg);},0);}"
			"document.location = window.location;"

			"var Audio = HTMLAudioElement;"
			"var Canvas = HTMLCanvasElement;"
			"var Image = HTMLImageElement;"
		;
		roVerify(JS_EvaluateScript(jsContext, jsGlobal, script, strlen(script), "", 0, NULL));
	}
}

static void appendFileToString(void* file, String& str)
{
	char buf[128];
	rhuint64 bytesRead = 0;
	while(fileSystem.read(file, buf, sizeof(buf), bytesRead))
		str.append(buf, num_cast<size_t>(bytesRead));
}

unsigned countNewline(const char* str, const char* end)
{
	unsigned count = 0;
	int len = end - str;
	while(--len >= 0)
		count += (str[len] == '\n') ? 1 : 0;
	return count;
}

bool Rhinoca::openDoucment(const char* uri)
{
	closeDocument();
	_initGlobal();

	String html;

	{	// Reads the html file into memory
		void* file = NULL;
		Status st = fileSystem.openFile(uri, file);
		if(!st) return false;
		appendFileToString(file, html);
		fileSystem.closeFile(file);
	}

	Dom::ElementFactory& factory = Dom::ElementFactory::singleton();
	Dom::HTMLDocument* document = domWindow->document;
	Dom::Node* currentNode = document;

	// Make documentUrl always in absolute path
	Path path = uri;
	if(!path.hasRootDirectory()) {
		path = Path::getCurrentPath() / path;
	}

	documentUrl = path.c_str();
	XmlParser parser;
	parser.parse(const_cast<char*>(html.c_str()));

	document->readyState = "loading";

	bool ended = false;
	while(!ended)
	{
		typedef XmlParser::Event Event;
		Event::Enum e = parser.nextEvent();
		switch(e)
		{
		case Event::BeginElement:
		{
			parser.pushAttributes();

			{
				const char* eleName = parser.elementName();
				Dom::Element* element = factory.create(this, eleName, &parser);
				if(!element) element = new Dom::Element(document->rhinoca);

				element->id = parser.attributeValueIgnoreCase("id");
				element->className = parser.attributeValueIgnoreCase("class");

				currentNode->appendChild(element);
				currentNode = element;
			}

			// Assign attributes from html tag to the HTMLElement javascript property
			for(unsigned i=0; i<parser.attributeCount(); ++i)
			{
				const char* name = parser.attributeName(i);
				const char* value = parser.attributeValue(i);

				// Make the attribute name case insensitive
				roToLower(const_cast<char*>(name));

				jsval v = STRING_TO_JSVAL(JS_NewStringCopyZ(jsContext, value));
				roVerify(JS_SetProperty(jsContext, *currentNode, name, &v));
			}

		}	break;

		case Event::EndElement:
		{
			parser.popAttributes();

			if(currentNode) {
				if(Dom::Element* ele = dynamic_cast<Dom::Element*>(currentNode))
					ele->onParserEndElement();

				currentNode = currentNode->parentNode;
			}
		}	break;

		case Event::Text:
		{
			Dom::TextNode* text = new Dom::TextNode(document->rhinoca);
			text->data = parser.textData();
			text->lineNumber = countNewline(html.c_str(), parser.textData());
			currentNode->appendChild(text);
		}	break;
		case Event::EndDocument:
			ended = true;
		default:
			break;
		}
	}

	// Perform selector match and assign style to matching elements
	ro::LinkList<Dom::CSSStyleSheet>& styleSheets = document->styleSheets;
	for(Dom::CSSStyleSheet* s = styleSheets.begin(); s != styleSheets.end(); s = s->next())
	{
		for(Dom::CSSStyleRule* r = s->rules.begin(); r != s->rules.end(); r = r->next())
		{
			r->selectorMatch(dynamic_cast<Dom::Element*>(document->firstChild));
		}
	}

	// Run window.onload
	{
		// TODO: The ready state should be interactive rather than complete
		document->readyState = "complete";

		Dom::Event* ev = new Dom::Event;
		ev->type = "DOMContentLoaded";
		ev->stopPropagation();	// Load events should not propagate
		ev->bubbles = false;
		ev->bind(jsContext, NULL);

		ev->target = domWindow;
		domWindow->dispatchEvent(ev);
		ev->target = document;
		document->dispatchEvent(ev);

		ev->type = "load";
		ev->target = domWindow;
		domWindow->dispatchEvent(ev);
		ev->target = document;
		document->dispatchEvent(ev);

		if(Dom::HTMLBodyElement* body = document->body()) {
			ev->target = body;
			// NOTE: We pass window as this object for body's onload callback
			body->dispatchEvent(ev, *domWindow);
		}
	}

	return true;
}

void Rhinoca::closeDocument()
{
//	subSystems.resourceMgr->abortAllLoader();

	// Clear all tasks before the VM shutdown, since any task would use the VM
//	subSystems.taskPool->waitAll();

	if(subSystems.audioDriver)
		subSystems.audioDriver->soundSourceStopAll(subSystems.audioDriver);

	roVerify(JS_RemoveObjectRoot(jsContext, &jsConsole));
	jsConsole = NULL;

	if(domWindow)
		domWindow->releaseGcRoot();

	documentUrl = "";
	domWindow = NULL;

	JS_GC(jsContext);
}
