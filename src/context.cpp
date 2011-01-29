#include "pch.h"
#include "context.h"
#include "path.h"
#include "platform.h"
#include "xmlparser.h"
#include "dom/document.h"
#include "dom/element.h"
#include "dom/keyevent.h"
#include "dom/mouseevent.h"
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
	print(rh, "JS %s: %s:%u\n%s\n",
		JSREPORT_IS_WARNING(report->flags) ? "warning" : "error",
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

	jsGlobal = JS_NewObject(jsContext, &jsGlobalClass, NULL, NULL);
	JS_SetGlobalObject(jsContext, jsGlobal);

	VERIFY(JS_InitStandardClasses(jsContext, jsGlobal));

	domWindow = new Dom::Window(this);
	domWindow->bind(jsContext, jsGlobal);
	domWindow->addGcRoot();	// releaseGcRoot() in ~Rhinoca()

	taskPool.init(1);
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
			"function alert(msg) { return window.alert(msg); }"
			"function setInterval(cb, interval) { return window.setInterval(cb, interval); };"
			"function setTimeout(cb, timeout) { return window.setTimeout(cb, timeout); };"
			"function clearInterval(cb) { window.clearInterval(cb); };"
			"function clearTimeout(cb) { window.clearTimeout(cb); };"
			"function log(msg){setTimeout(function(){throw new Error(msg);},0);}"

			"var navigator = { userAgent: {"
			"	indexOf : function(str) { return -1; }, match : function(str) { return false; }"
			"}};"
		;
		jsval rval;
		VERIFY(JS_EvaluateScript(jsContext, jsGlobal, script, strlen(script), "", 0, &rval));
	}
}

Rhinoca::~Rhinoca()
{
	VERIFY(JS_RemoveRoot(jsContext, &jsConsole));

	domWindow->releaseGcRoot();

	// NOTE: I found this "clear scope" is necessary in order to not leak memory
	// This requirement is not mentioned in the Mozilla documentation, I wonder why
	// JS_DestroyContext() didn't do all the necessary job to clear the global.
	// Similar problem: http://web.archiveorange.com/archive/v/yxPWTpZYQx37ab1hpyWc
	JS_ClearScope(jsContext, jsGlobal);

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

	JS_MaybeGC(jsContext);
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

	const char* mouseEvent = NULL;

	switch(ev.type)
	{
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
		if(/*myKeyRepeatEnabled || */((lParam & (1 << 30)) == 0)) {
			keyCode = convertKeyCode(wParam, lParam);
			keyEvent = onkeydown;
		}
		break;

	case WM_KEYUP:
	case WM_SYSKEYUP:
		keyCode = convertKeyCode(wParam, lParam);
		keyEvent = onkeyup;
		break;

	case WM_LBUTTONDOWN:
		mouseEvent = "onmousedown";
		break;
	case WM_LBUTTONUP:
		mouseEvent = "onmouseup";
		break;
	case WM_MOUSEMOVE:
		mouseEvent = "onmousemove";
		break;
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

	if(mouseEvent) for(Dom::NodeIterator itr(domWindow->document->rootNode()); !itr.ended(); itr.next())
	{
		jsval argv, closure, rval;
		if(JS_GetProperty(itr->jsContext, itr->jsObject, mouseEvent, &closure) && closure != JSVAL_VOID) {
			Dom::MouseEvent* e = new Dom::MouseEvent;
			e->clientX = LOWORD(lParam);
			e->clientY = HIWORD(lParam);

//			printf("%d, %d\n", e->clientX, e->clientY);

			e->bind(itr->jsContext, NULL);
			argv = OBJECT_TO_JSVAL(e->jsObject);
			JS_CallFunctionValue(itr->jsContext, itr->jsObject, closure, 1, &argv, &rval);
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

const char* removeBom(Rhinoca* rh, const char* uri, const char* str, unsigned& len)
{
	if(len < 3) return str;

	if( (str[0] == (char)0xFE && str[1] == (char)0xFF) ||
		(str[0] == (char)0xFF && str[1] == (char)0xFE))
	{
		print(rh, "'%s' is encode using UTF-16 which is not supported\n");
		len = 0;
		return NULL;
	}

	if(str[0] == (char)0xEF && str[1] == (char)0xBB && str[2] == (char)0xBF) {
		len -= 3;
		return str + 3;
	}
	return str;
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
				const char* eleName = parser.elementName();
				Dom::Element* element = factory.create(this, eleName, &parser);
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
				const char* srcUrl = NULL;

				if(srcUrl = parser.attributeValueIgnoreCase("src"))
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
					unsigned len = script.size();
					if(const char* s = removeBom(this, srcUrl, script.c_str(), len)) {
						jsval rval;
						JS_EvaluateScript(jsContext, jsGlobal, s, len, scriptUrl.c_str(), lineNo+1, &rval);
					}
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
