#ifndef __DOM_IMAGE_H__
#define __DOM_IMAGE_H__

#include "element.h"

namespace Render {

typedef IntrusivePtr<class Texture> TexturePtr;

}	// namespace Render

namespace Dom {

class HTMLImageElement : public Element
{
public:
	HTMLImageElement();
	~HTMLImageElement();

// Operations
	void bind(JSContext* cx, JSObject* parent);

	static void registerClass(JSContext* cx, JSObject* parent);
	static Element* factoryCreate(Rhinoca* rh, const char* type, XmlParser* parser);

// Attributes
	Render::TexturePtr texture;

	void setSrc(Rhinoca* rh, const char* uri);

	unsigned width() const;
	unsigned height() const;
	unsigned naturalWidth() const;
	unsigned naturalHeight() const;

	override const char* tagName() const;

	static JSClass jsClass;

	int _width, _height;
};	// HTMLImageElement

}	// namespace Dom

#endif	// __DOME_IMAGE_H__
