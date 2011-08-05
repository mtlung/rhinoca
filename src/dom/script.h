#ifndef __DOM_SCRIPT_H__
#define __DOM_SCRIPT_H__

#include "element.h"
#include "../vector.h"

namespace Dom {

class HTMLScriptElement : public Element
{
public:
	explicit HTMLScriptElement(Rhinoca* rh);
	~HTMLScriptElement();

// Operations
	override void bind(JSContext* cx, JSObject* parent);

	override void onParserEndElement();

	void runScript(const char* code);

	static Element* factoryCreate(Rhinoca* rh, const char* type, XmlParser* parser);

// Attributes
	bool inited;

	void setSrc(const char* uri);
	const FixString& src() const;

	override const FixString& tagName() const;

	static JSClass jsClass;

	FixString _src;
};	// HTMLScriptElement

}	// namespace Dom

#endif	// __DOM_SCRIPT_H__
