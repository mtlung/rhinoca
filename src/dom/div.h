#ifndef __DOM_DIV_H__
#define __DOM_DIV_H__

#include "element.h"

namespace Dom {

class HTMLDivElement : public Element
{
public:
	explicit HTMLDivElement(Rhinoca* rh);
	~HTMLDivElement();

// Operations
	void bind(JSContext* cx, JSObject* parent) override;

	static Element* factoryCreate(Rhinoca* rh, const char* type, XmlParser* parser);

// Attributes
	const ro::ConstString& tagName() const override;

	static JSClass jsClass;
};	// HTMLDivElement

}	// namespace Dom

#endif	// __DOM_DIV_H__
