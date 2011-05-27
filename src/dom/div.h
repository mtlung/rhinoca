#ifndef __DOM_DIV_H__
#define __DOM_DIV_H__

#include "element.h"

namespace Dom {

class HTMLDivElement : public Element
{
public:
	HTMLDivElement();
	~HTMLDivElement();

// Operations
	override void bind(JSContext* cx, JSObject* parent);

	static Element* factoryCreate(Rhinoca* rh, const char* type, XmlParser* parser);

// Attributes
	override const FixString& tagName() const;

	static JSClass jsClass;
};	// HTMLDivElement

}	// namespace Dom

#endif	// __DOM_DIV_H__
