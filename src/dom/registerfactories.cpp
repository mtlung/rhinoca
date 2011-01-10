#include "pch.h"
#include "canvas.h"
#include "image.h"

namespace Dom {

void registerClasses(JSContext* cx, JSObject* parent)
{
	Image::registerClass(cx, parent);
}

void registerFactories()
{
	ElementFactory& f = ElementFactory::singleton();

	f.addFactory(&Canvas::factoryCreate);
	f.addFactory(&Image::factoryCreate);
}

}	// namespace Dom
