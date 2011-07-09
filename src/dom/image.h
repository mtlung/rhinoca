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
	explicit HTMLImageElement(Rhinoca* rh);
	~HTMLImageElement();

// Operations
	override void bind(JSContext* cx, JSObject* parent);

	static void registerClass(JSContext* cx, JSObject* parent);
	static Element* factoryCreate(Rhinoca* rh, const char* type, XmlParser* parser);

// Attributes
	Render::TexturePtr texture;

	void setSrc(const char* uri);

	unsigned width() const;
	unsigned height() const;

	void setWidth(unsigned w);
	void setHeight(unsigned h);

	unsigned naturalWidth() const;
	unsigned naturalHeight() const;

	/// Use enum values as seen in Render::Driver::SamplerState
	/// Can be MIN_MAG_LINEAR (default) or MIN_MAG_POINT
	int filter;

	override const FixString& tagName() const;

	static JSClass jsClass;

	int _width, _height;
};	// HTMLImageElement

}	// namespace Dom

#endif	// __DOM_IMAGE_H__
