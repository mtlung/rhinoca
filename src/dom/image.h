#ifndef __DOM_IMAGE_H__
#define __DOM_IMAGE_H__

#include "element.h"
#include "../../roar/base/roSharedPtr.h"
#include "../../roar/render/roTexture.h"

namespace Dom {

class HTMLImageElement : public Element
{
public:
	explicit HTMLImageElement(Rhinoca* rh);
	~HTMLImageElement();

// Operations
	void bind(JSContext* cx, JSObject* parent) override;

	static void registerClass(JSContext* cx, JSObject* parent);
	static Element* factoryCreate(Rhinoca* rh, const char* type, XmlParser* parser);

// Attributes
	ro::TexturePtr texture;
//	Render::TexturePtr texture;

	void setSrc(const char* uri);

	unsigned width() const override;
	unsigned height() const override;

	unsigned naturalWidth() const;
	unsigned naturalHeight() const;

	void setWidth(unsigned w) override;
	void setHeight(unsigned h) override;

	/// Use enum values as seen in Render::Driver::SamplerState
	/// Can be MIN_MAG_LINEAR (default) or MIN_MAG_POINT
	int filter;

	const ro::ConstString& tagName() const override;

	static JSClass jsClass;
};	// HTMLImageElement

}	// namespace Dom

#endif	// __DOM_IMAGE_H__
