#include "pch.h"
#include "context.h"
#include "path.h"
#include "platform.h"
#include "xmlparser.h"
#include "dom/document.h"
#include "dom/element.h"
#include "dom/keyevent.h"
#include "dom/registerfactories.h"
#include "loader/loader.h"

#include <string>

#ifdef RHINOCA_VC
#	ifndef NDEBUG
#		pragma comment(lib, "js32d")
#	else
#		pragma comment(lib, "js32")
#	endif
#endif

void jsReportError(JSContext* cx, const char* message, JSErrorReport* report)
{
	Rhinoca* rh = reinterpret_cast<Rhinoca*>(JS_GetContextPrivate(cx));
	print(rh, "JS error: %s:%u\n%s\n",
		report->filename ? report->filename : "<no filename>",
		(unsigned int)report->lineno,
		message
	);
}

static JSClass jsGlobalClass = {
	"global", JSCLASS_GLOBAL_FLAGS,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
	JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSClass jsConsoleClass = {
	"console", JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
	JSCLASS_NO_OPTIONAL_MEMBERS
};

JSBool jsConsoleLog(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval)
{
	if(JSString* jss = JS_ValueToString(cx, argv[0])) {
		char* str = JS_GetStringBytes(jss);
		Rhinoca* rh = reinterpret_cast<Rhinoca*>(JS_GetPrivate(cx, obj));
		print(rh, "%s", str);
		return JS_TRUE;
	}

	return JS_FALSE;
}

static JSFunctionSpec jsConsoleMethods[] = {
	{"log", jsConsoleLog, 1,0,0},
	{0}
};

Rhinoca::Rhinoca()
	: privateData(NULL)
	, width(0), height(0)
	, renderContex(NULL)
{
	jsContext = JS_NewContext(jsrt, 8192);
	JS_SetOptions(jsContext, JS_GetOptions(jsContext) | JSOPTION_STRICT);
	JS_SetContextPrivate(jsContext, this);
	JS_SetErrorReporter(jsContext, jsReportError);

	jsGlobal = JS_NewObject(jsContext, &jsGlobalClass, 0, 0);
	JS_SetGlobalObject(jsContext, jsGlobal);

	VERIFY(JS_InitStandardClasses(jsContext, jsGlobal));

	domWindow = new Dom::Window(this);
	domWindow->bind(jsContext, jsGlobal);
	domWindow->addGcRoot();	// releaseGcRoot() in ~Rhinoca()

//	taskPool.init(3);
	resourceManager.taskPool = &taskPool;
	Loader::registerLoaders(&resourceManager);

	Dom::registerFactories();
	Dom::registerClasses(jsContext, jsGlobal);

	{	// Register the console object
		jsConsole = JS_DefineObject(jsContext, jsGlobal, "console", &jsConsoleClass, 0, JSPROP_ENUMERATE);
		VERIFY(JS_SetPrivate(jsContext, jsConsole, this));
		VERIFY(JS_AddNamedRoot(jsContext, &jsConsole, "console"));
		VERIFY(JS_DefineFunctions(jsContext, jsConsole, jsConsoleMethods));
	}

	{	// Run some default scripts
		const char* script =
			"function setInterval(cb, interval) { return window.setInterval(cb, interval); };"
			"function setTimeout(cb, timeout) { return window.setTimeout(cb, timeout); };"
			"function clearInterval(cb) { window.clearInterval(cb); };"
			"function clearTimeout(cb) { window.clearTimeout(cb); };"
			"function log(msg){setTimeout(function(){throw new Error(msg);},0);}"
		;
		jsval rval;
		VERIFY(JS_EvaluateScript(jsContext, jsGlobal, script, strlen(script), "", 0, &rval));
	}
}

Rhinoca::~Rhinoca()
{
	VERIFY(JS_RemoveRoot(jsContext, &jsConsole));
	domWindow->releaseGcRoot();

	// TODO: I've got problem that JS_DestroyContext() will not release global variables
	const char* script = "for(att in this) { delete this[att]; }";
	jsval rval;
	VERIFY(JS_EvaluateScript(jsContext, jsGlobal, script, strlen(script), "", 0, &rval));

//	FILE* f = fopen("dumpHeap.txt", "w");
//	JS_DumpHeap(jsContext, f, NULL, 0, NULL, 1000, NULL);
//	fclose(f);

	JS_DestroyContext(jsContext);
}

void Rhinoca::update()
{
	domWindow->update();
	resourceManager.update();
	taskPool.doSomeTask();
}

int convertKeyCode(WPARAM virtualKey, LPARAM flags)
{
	switch(virtualKey)
	{
		case VK_SPACE :			return 32;
		case VK_LEFT :			return 37;
		case VK_RIGHT :			return 39;
		case VK_UP :			return 38;
		case VK_DOWN :			return 0;
	}

	return 0;
}

void Rhinoca::processEvent(RhinocaEvent ev)
{
	WPARAM wParam = ev.value1;
	LPARAM lParam = ev.value2;

	const char* onkeydown = "onkeydown";
	const char* onkeyup = "onkeyup";
	const char* keyEvent = NULL;
	int keyCode = 0;

	switch(ev.type)
	{
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
	{
		if(/*myKeyRepeatEnabled || */((lParam & (1 << 30)) == 0)) {
			keyCode = convertKeyCode(wParam, lParam);
			keyEvent = onkeydown;
		}

		break;
	}

	case WM_KEYUP:
	case WM_SYSKEYUP:
		keyCode = convertKeyCode(wParam, lParam);
		keyEvent = onkeyup;
	}

	if(keyEvent)
	{
		jsval argv, closure, rval;
		Dom::Document* document = domWindow->document;
		if(JS_GetProperty(document->jsContext, document->jsObject, keyEvent, &closure) && closure != JSVAL_VOID) {
			Dom::KeyEvent* e = new Dom::KeyEvent;
			e->keyCode = keyCode;
			e->bind(document->jsContext, NULL);
			argv = OBJECT_TO_JSVAL(e->jsObject);
			JS_CallFunctionValue(document->jsContext, document->jsObject, closure, 1, &argv, &rval);
		}
	}
}

void Rhinoca::collectGarbage()
{
	JS_GC(jsContext);
}

static void appendFileToString(void* file, std::string& str)
{
	char buf[128];
	rhuint64 readCount;
	while(readCount = io_read(file, buf, sizeof(buf), 0))
		str.append(buf, (size_t)readCount);
}

unsigned countNewline(const char* str, const char* end)
{
	const char* p = str;
	unsigned count = 0;
	int len = end - str;
	while(--len >= 0)
		count += (str[len] == '\n') ? 1 : 0;
	return count;
}

bool Rhinoca::openDoucment(const char* uri)
{
	std::string html;

	{	// Reads the html file into memory
		void* file = io_open(this, uri, 0);
		if(!file) return false;
		appendFileToString(file, html);
		io_close(file, 0);
	}

	Dom::ElementFactory& factory = Dom::ElementFactory::singleton();
	Dom::Node* currentNode = domWindow->document->rootNode();

	documentUrl = uri;
	XmlParser parser;
	parser.parse(const_cast<char*>(html.c_str()));

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
				Dom::Element* element = factory.create(this, parser.elementName(), &parser);
				if(!element) element = new Dom::Element;

				element->ownerDocument = domWindow->document;
				element->id = parser.attributeValueIgnoreCase("id");

				currentNode->appendChild(element);
				currentNode = element;
			}

			if(strcasecmp(parser.elementName(), "body") == 0)
			{
				if(const char* str = parser.attributeValueIgnoreCase("onload")) {
					std::string script = "window.onload=function(){";
					script += str;
					script += "};";
					jsval rval;
					JS_EvaluateScript(jsContext, jsGlobal, script.c_str(), script.size(), uri, 0, &rval);
				}
			}
			else if(strcasecmp(parser.elementName(), "canvas") == 0)
			{
			}
			else if(strcasecmp(parser.elementName(), "script") == 0)
			{
			}
		}	break;

		case Event::EndElement:
		{
			parser.popAttributes();
			if(strcasecmp(parser.elementName(), "body") == 0)
			{
				if(const char* str = parser.attributeValueIgnoreCase("onload")) {
					std::string script = "window.onload=function(){";
					script += str;
					script += "};";
					jsval rval;
					JS_EvaluateScript(jsContext, jsGlobal, script.c_str(), script.size(), uri, 0, &rval);
				}
			}
			else if(strcasecmp(parser.elementName(), "script") == 0)
			{
				std::string script;
				Path scriptUrl = documentUrl.c_str();
				unsigned lineNo = 0;

				if(const char* srcUrl = parser.attributeValueIgnoreCase("src"))
				{
					scriptUrl = scriptUrl.getBranchPath() / srcUrl;
					if(void* file = io_open(this, scriptUrl.c_str(), 0)) {
						appendFileToString(file, script);
						io_close(file, 0);
					}
				}
				else {
					script = parser.textData();
					lineNo = countNewline(html.c_str(), parser.textData());
				}

				if(!script.empty()) {
					jsval rval;
					JS_EvaluateScript(jsContext, jsGlobal, script.c_str(), script.size(), scriptUrl.c_str(), lineNo+1, &rval);
				}
			}

			if(currentNode)
				currentNode = currentNode->parentNode;
		}	break;

		case Event::Text:
			break;
		case Event::EndDocument:
			ended = true;
		default:
			break;
		}
	}

	// Run windows.onload
	{
		jsval rval;
		jsval closure;
		if(JS_GetProperty(jsContext, domWindow->jsObject, "onload", &closure) && closure != JSVAL_VOID)
			JS_CallFunctionValue(jsContext, domWindow->jsObject, closure, 0, NULL, &rval);
	}

	return true;
}

void Rhinoca::closeDocument()
{
}
