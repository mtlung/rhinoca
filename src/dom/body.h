#ifndef __DOM_BODY_H__
#define __DOM_BODY_H__

#include "element.h"

namespace Dom {

class HTMLBodyElement : public Element
{
public:
	HTMLBodyElement();
	~HTMLBodyElement();

// Operations
	void bind(JSContext* cx, JSObject* parent);

	static Element* factoryCreate(Rhinoca* rh, const char* type, XmlParser* parser);

// Attributes
	override const FixString& tagName() const;

	static JSClass jsClass;
};	// HTMLBodyElement

}	// namespace Dom

#endif	// __DOM_BODY_H__
