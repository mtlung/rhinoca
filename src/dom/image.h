#ifndef __DOME_IMAGE_H__
#define __DOME_IMAGE_H__

#include "element.h"

namespace Dom {

class Image : public Element
{
public:
	Image();
	~Image();

// Operations
	void bind(JSContext* cx, JSObject* parent);

	static void registerClass(JSContext* cx, JSObject* parent);
	static Element* factoryCreate(const char* type);

// Attributes
	int width() const { return _width; }
	int height() const { return _height; }

	void setWidth();
	void setHeight();

	static JSClass jsClass;

protected:
	int _width;
	int _height;
};	// Canvas

}	// namespace Dom

#endif	// __DOME_IMAGE_H__
