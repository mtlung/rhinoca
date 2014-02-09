#ifndef __DOM_BODY_H__
#define __DOM_BODY_H__

#include "element.h"

namespace Dom {

class HTMLBodyElement : public Element
{
public:
	explicit HTMLBodyElement(Rhinoca* rh);
	~HTMLBodyElement();

// Operations
	void bind(JSContext* cx, JSObject* parent) override;

	static Element* factoryCreate(Rhinoca* rh, const char* type, XmlParser* parser);

// Attributes
	const ro::ConstString& tagName() const override;

	static JSClass jsClass;
};	// HTMLBodyElement

}	// namespace Dom

#endif	// __DOM_BODY_H__
