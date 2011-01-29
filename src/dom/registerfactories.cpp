#include "pch.h"
#include "canvas.h"
#include "image.h"

namespace Dom {

void registerClasses(JSContext* cx, JSObject* parent)
{
	HTMLCanvasElement::registerClass(cx, parent);
	HTMLImageElement::registerClass(cx, parent);
}

void registerFactories()
{
	ElementFactory& f = ElementFactory::singleton();

	f.addFactory(&HTMLCanvasElement::factoryCreate);
	f.addFactory(&HTMLImageElement::factoryCreate);
}

}	// namespace Dom
