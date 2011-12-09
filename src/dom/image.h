#ifndef __DOM_IMAGE_H__
#define __DOM_IMAGE_H__

#include "element.h"
#include "../../roar/base/roSharedPtr.h"

namespace Render {

typedef ro::SharedPtr<class Texture> TexturePtr;

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

	override unsigned width() const;
	override unsigned height() const;

	unsigned naturalWidth() const;
	unsigned naturalHeight() const;

	override void setWidth(unsigned w);
	override void setHeight(unsigned h);

	/// Use enum values as seen in Render::Driver::SamplerState
	/// Can be MIN_MAG_LINEAR (default) or MIN_MAG_POINT
	int filter;

	override const ro::ConstString& tagName() const;

	static JSClass jsClass;
};	// HTMLImageElement

}	// namespace Dom

#endif	// __DOM_IMAGE_H__
