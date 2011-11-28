#ifndef __DOM_STYLE_H__
#define __DOM_STYLE_H__

#include "element.h"
#include "../vector.h"

namespace Dom {

class HTMLStyleElement : public Element
{
public:
	explicit HTMLStyleElement(Rhinoca* rh);
	~HTMLStyleElement();

// Operations
	override void bind(JSContext* cx, JSObject* parent);

	override void onParserEndElement();

	static Element* factoryCreate(Rhinoca* rh, const char* type, XmlParser* parser);

// Attributes
	struct String { const char* begin, *end; };
	struct Selector { Vector<String> tokens; };
	struct PropertyDecl { String name, value; };

	struct Rule
	{
		Vector<Selector> selectors;	/// List of selectors that use this declarations
		Vector<PropertyDecl> declarations;
	};

	Vector<Rule> rules;

	char* source;

	override const ro::ConstString& tagName() const;

	static JSClass jsClass;
};	// HTMLStyleElement

}	// namespace Dom

#endif	// __DOM_STYLE_H__
