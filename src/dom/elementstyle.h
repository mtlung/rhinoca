#ifndef __CSS_ELEMENTSTYLE_H__
#define __CSS_ELEMENTSTYLE_H__

#include "../jsbindable.h"
#include "../render/texture.h"

namespace Dom {

class Element;

class ElementStyle : public JsBindable
{
public:
	explicit ElementStyle(Dom::Element* ele);
	~ElementStyle();

	// Operations
	override void bind(JSContext* cx, JSObject* parent);

	// Attributes
	Dom::Element* element;

	/// Visible vs Display:
	/// In CSS: visibility = "visible|hidden", display = "inline|block|none"
	/// Non visible element will not receive events but still occupy space.
	/// See: http://www.w3schools.com/css/css_display_visibility.asp
	/// Since we don't support layout, we just have a single boolean for visible or not.
	bool visible() const;
	void setVisible(bool val);

	/// All dimension stuffs are in unit of pixels
	int left() const;
	void setLeft(int val);

	int top() const;
	void setTop(int val);

	unsigned width() const;
	void setWidth(unsigned val);

	unsigned height() const;
	void setHeight(unsigned val);

	float backgroundPositionX;
	float backgroundPositionY;
	Render::TexturePtr backgroundImage;

	static JSClass jsClass;
};	// ElementStyle

}	// namespace Dom

#endif	// __CSS_ELEMENTSTYLE_H__
