#ifndef __DOM_TEXTNODE_H__
#define __DOM_TEXTNODE_H__

#include "element.h"

namespace Dom {

class TextNode : public Node
{
public:
	explicit TextNode(Rhinoca* rh);
	~TextNode();

// Operations
	override void bind(JSContext* cx, JSObject* parent);

// Attributes
	/// The line number where this text node begin in the document
	unsigned lineNumber;

	ro::String data;

	static JSClass jsClass;
};	// TextNode

}	// namespace Dom

#endif	// __DOM_TEXTNODE_H__
