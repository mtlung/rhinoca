#include "pch.h"
#include "audio.h"
#include "body.h"
#include "canvas.h"
#include "div.h"
#include "image.h"
#include "window.h"

namespace Dom {

void registerClasses(JSContext* cx, JSObject* parent)
{
	Element::registerClass(cx, parent);
	HTMLAudioElement::registerClass(cx, parent);
	HTMLCanvasElement::registerClass(cx, parent);
	HTMLImageElement::registerClass(cx, parent);
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
}

}	// namespace Dom
