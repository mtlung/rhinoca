#include "pch.h"
#include "context.h"
#include "path.h"
#include "xmlparser.h"
#include "dom/document.h"
#include "dom/element.h"
#include "dom/registerfactories.h"

#include <string>

#ifndef NDEBUG
#	pragma comment(lib, "js32d")
#else
#	pragma comment(lib, "js32")
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
{
	jsContext = JS_NewContext(jsrt, 8192);
	JS_SetContextPrivate(jsContext, this);
	JS_SetErrorReporter(jsContext, jsReportError);

	jsGlobal = JS_NewObject(jsContext, &jsGlobalClass, 0, 0);
	VERIFY(JS_InitStandardClasses(jsContext, jsGlobal));

	domWindow = new Dom::Window;
	domWindow->bind(jsContext, jsGlobal);
	domWindow->addGcRoot();

	Dom::registerFactories();
	Dom::registerClasses(jsContext, jsGlobal);

	{	// Register the console object
		jsConsole = JS_DefineObject(jsContext, jsGlobal, "console", &jsConsoleClass, 0, 0);
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
	const char* script = "document=null;window=null;";
	jsval rval;
	VERIFY(JS_EvaluateScript(jsContext, jsGlobal, script, strlen(script), "", 0, &rval));
	JS_DestroyContext(jsContext);
}

void Rhinoca::update()
{
	domWindow->update();
}

void Rhinoca:: collectGarbage()
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

bool Rhinoca::openDoucment(const char* url)
{
	std::string html;

	{	// Reads the html file into memory
		void* file = io_open(this, url, 0);
		if(!file) return false;
		appendFileToString(file, html);
		io_close(file, 0);
	}

	Dom::ElementFactory& factory = Dom::ElementFactory::singleton();
	Dom::Node* currentNode = domWindow->document->rootNode();

	documentUrl = url;
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
				Dom::Element* element = factory.create(parser.elementName());
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
					JS_EvaluateScript(jsContext, jsGlobal, script.c_str(), script.size(), "", 0, &rval);
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
					JS_EvaluateScript(jsContext, jsGlobal, script.c_str(), script.size(), "", 0, &rval);
				}
			}
			else if(strcasecmp(parser.elementName(), "script") == 0)
			{
				std::string script;
				Path scriptUrl = documentUrl.c_str();

				if(const char* srcUrl = parser.attributeValueIgnoreCase("src"))
				{
					scriptUrl = scriptUrl.getBranchPath() / srcUrl;
					if(void* file = io_open(this, scriptUrl.c_str(), 0)) {
						appendFileToString(file, script);
						io_close(file, 0);
					}
				}
				else
					script = parser.textData();

				if(!script.empty()) {
					jsval rval;
					JS_EvaluateScript(jsContext, jsGlobal, script.c_str(), script.size(), scriptUrl.c_str(), 0, &rval);
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
