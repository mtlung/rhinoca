#ifndef __DOM_SCRIPT_H__
#define __DOM_SCRIPT_H__

#include "element.h"
#include "../textresource.h"

namespace Dom {

class HTMLScriptElement : public Element
{
public:
	explicit HTMLScriptElement(Rhinoca* rh);
	~HTMLScriptElement();

// Operations
	override void bind(JSContext* cx, JSObject* parent);

	override void onParserEndElement();

	bool runScript(const char* code, const char* srcPath=NULL, unsigned lineNumber=0);

	static void registerClass(JSContext* cx, JSObject* parent);
	static Element* factoryCreate(Rhinoca* rh, const char* type, XmlParser* parser);

// Attributes
	bool inited;

	void setSrc(const char* uri);
	const char* src() const;	/// The path of the js file, not the context of the script

	override const ro::ConstString& tagName() const;

	TextResourcePtr _src;

	static JSClass jsClass;
};	// HTMLScriptElement

}	// namespace Dom

#endif	// __DOM_SCRIPT_H__
