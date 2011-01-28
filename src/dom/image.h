#ifndef __DOM_IMAGE_H__
#define __DOM_IMAGE_H__

#include "element.h"

namespace Render {

typedef IntrusivePtr<class Texture> TexturePtr;

}	// namespace Render

namespace Dom {

class Image : public Element
{
public:
	Image();
	~Image();

// Operations
	void bind(JSContext* cx, JSObject* parent);

	static void registerClass(JSContext* cx, JSObject* parent);
	static Element* factoryCreate(Rhinoca* rh, const char* type, XmlParser* parser);

// Attributes
	Render::TexturePtr texture;

	unsigned width() const;
	unsigned height() const;
	unsigned naturalWidth() const;
	unsigned naturalHeight() const;

	static JSClass jsClass;

	int _width, _height;
};	// Image

}	// namespace Dom

#endif	// __DOME_IMAGE_H__
