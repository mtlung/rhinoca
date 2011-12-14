#include "pch.h"
#include "context.h"
#include "common.h"
#include "loader.h"
#include "path.h"
#include "platform.h"
#include "rhinoca.h"
#include "xmlparser.h"
#include "audio/audiodevice.h"
#include "dom/body.h"
#include "dom/element.h"
#include "dom/event.h"
#include "dom/document.h"
#include "dom/keyevent.h"
#include "dom/mouseevent.h"
#include "dom/textnode.h"
#include "dom/touchevent.h"
#include "dom/registerfactories.h"
#include "render/driver.h"
#include "render/texture.h"
#include "../roar/base/roFileSystem.h"
#include "../roar/base/roLog.h"

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

Rhinoca::Rhinoca(RhinocaRenderContext* rc)
	: privateData(NULL)
	, domWindow(NULL)
	, width(0), height(0)
	, renderContex(rc)
	, audioDevice(NULL)
	, fps(1)
	, frameTime(1)
	, _lastFrameTime(0)
	, _gcFrameIntervalCounter(_gcFrameInterval)
{
	jsContext = JS_NewContext(jsrt, 8192);
	JS_SetOptions(jsContext, JS_GetOptions(jsContext) | JSOPTION_JIT);
//	JS_SetOptions(jsContext, JS_GetOptions(jsContext) | JSOPTION_STRICT);
//	JS_SetOptions(jsContext, JS_GetOptions(jsContext) | JSOPTION_VAROBJFIX);
	JS_SetContextPrivate(jsContext, this);
	JS_SetErrorReporter(jsContext, jsReportError);

	taskPool.init(2);
	resourceManager.taskPool = &taskPool;
	Loader::registerLoaders(&resourceManager);

	audioDevice = audiodevice_create();

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

	audiodevice_destroy(audioDevice);
}

void Rhinoca::update()
{
	Render::Driver::forceApplyCurrent();

	if(domWindow)
		domWindow->update();

	resourceManager.tick();
	resourceManager.collectInfrequentlyUsed();

	audiodevice_update(audioDevice);

	taskPool.doSomeTask(1.0f / 60);

	if(domWindow) {
		domWindow->render();

		if(--_gcFrameIntervalCounter == 0) {
			_gcFrameIntervalCounter = _gcFrameInterval;
			JS_MaybeGC(jsContext);
		}
	}

	Render::Driver::useRenderTarget(NULL);

	// FPS calculation
	float currentTime = _stopWatch.getFloat();
	frameTime = currentTime - _lastFrameTime;
	fps = (1.0f / frameTime) * 0.1f + fps * 0.9f;
//	printf("\rfps: %f", fps);
	_lastFrameTime = currentTime;
}

void Rhinoca::collectGarbage()
{
	JS_GC(jsContext);
}

void Rhinoca::_initGlobal()
{
	RHASSERT(!domWindow);

	// NOTE: We use the Window object as the global
	jsGlobal = JS_NewCompartmentAndGlobalObject(jsContext, &Dom::Window::jsClass, NULL);
	JS_SetGlobalObject(jsContext, jsGlobal);

	RHVERIFY(JS_InitStandardClasses(jsContext, jsGlobal));

	Dom::registerClasses(jsContext, jsGlobal);

	domWindow = new Dom::Window(this);
	domWindow->jsObject = jsGlobal;
	domWindow->bind(jsContext, jsGlobal);
	domWindow->addGcRoot();	// releaseGcRoot() in ~Rhinoca()

	{	// Register the console object
		jsConsole = JS_DefineObject(jsContext, jsGlobal, "console", &jsConsoleClass, 0, JSPROP_ENUMERATE);
		RHVERIFY(JS_SetPrivate(jsContext, jsConsole, this));
		RHVERIFY(JS_AddNamedObjectRoot(jsContext, &jsConsole, "console"));
		RHVERIFY(JS_DefineFunctions(jsContext, jsConsole, jsConsoleMethods));
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
		RHVERIFY(JS_EvaluateScript(jsContext, jsGlobal, script, strlen(script), "", 0, NULL));
	}
}

static void appendFileToString(void* file, String& str)
{
	char buf[128];
	rhuint64 readCount;
	while((readCount = fileSystem.read(file, buf, sizeof(buf))))
		str.append(buf, (size_t)readCount);
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
		void* file = fileSystem.openFile(uri);
		if(!file) return false;
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
				RHVERIFY(JS_SetProperty(jsContext, *currentNode, name, &v));
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
	resourceManager.abortAllLoader();

	// Clear all tasks before the VM shutdown, since any task would use the VM
	taskPool.waitAll();

	RHVERIFY(JS_RemoveObjectRoot(jsContext, &jsConsole));
	jsConsole = NULL;

	if(domWindow)
		domWindow->releaseGcRoot();

	documentUrl = "";
	domWindow = NULL;

	JS_GC(jsContext);
}
