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
	override void bind(JSContext* cx, JSObject* parent);

	static Element* factoryCreate(Rhinoca* rh, const char* type, XmlParser* parser);

// Attributes
	override const FixString& tagName() const;

	static JSClass jsClass;
};	// HTMLBodyElement

}	// namespace Dom

#endif	// __DOM_BODY_H__
