#include "pch.h"
#include "audio.h"
#include "body.h"
#include "canvas.h"
#include "canvas2dcontext.h"
#include "style.h"
#include "div.h"
#include "image.h"
#include "script.h"
#include "window.h"

namespace Dom {

void registerClasses(JSContext* cx, JSObject* parent)
{
	Element::registerClass(cx, parent);
	HTMLAudioElement::registerClass(cx, parent);
	HTMLCanvasElement::registerClass(cx, parent);
	CanvasRenderingContext2D::registerClass(cx, parent);
	HTMLImageElement::registerClass(cx, parent);
	HTMLScriptElement::registerClass(cx, parent);
	Window::registerClass(cx, parent);
}

void registerFactories()
{
	ElementFactory& f = ElementFactory::singleton();

	f.addFactory(&HTMLAudioElement::factoryCreate);
	f.addFactory(&HTMLBodyElement::factoryCreate);
	f.addFactory(&HTMLCanvasElement::factoryCreate);
	f.addFactory(&HTMLDivElement::factoryCreate);
	f.addFactory(&HTMLImageElement::factoryCreate);
	f.addFactory(&HTMLScriptElement::factoryCreate);
	f.addFactory(&HTMLStyleElement::factoryCreate);
}

}	// namespace Dom
