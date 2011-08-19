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

	void setStyleString(const char* begin, const char* end);

	void setStyleAttribute(const char* name, const char* value);

	/// Visible vs Display:
	/// In CSS: visibility = "visible|hidden", display = "inline|block|none"
	/// Non visible element will not receive events but still occupy space.
	/// See: http://www.w3schools.com/css/css_display_visibility.asp
	/// Since we don't support layout, we just have a single boolean for visible or not.
	bool visible() const;
	void setVisible(bool val);

	/// All dimension stuffs are in unit of pixels
	float left() const;
	void setLeft(float val);

	float top() const;
	void setTop(float val);

	unsigned width() const;
	void setWidth(unsigned val);

	unsigned height() const;
	void setHeight(unsigned val);

	float backgroundPositionX;
	float backgroundPositionY;

	bool setBackgroundImage(const char* cssUrl);
	Render::TexturePtr backgroundImage;

	static JSClass jsClass;
};	// ElementStyle

}	// namespace Dom

#endif	// __CSS_ELEMENTSTYLE_H__
