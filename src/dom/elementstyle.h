#ifndef __CSS_ELEMENTSTYLE_H__
#define __CSS_ELEMENTSTYLE_H__

#include "color.h"
#include "../jsbindable.h"
#include "../render/texture.h"
#include "../mat44.h"
#include "../vec3.h"

namespace Dom {

class Element;

class ElementStyle : public JsBindable
{
public:
	explicit ElementStyle(Dom::Element* ele);
	~ElementStyle();

// Operations
	/// Calculate it's _localToWorld matrix
	/// For this function to work, you must call this function for an Element's parent first
	void updateTransformation();

	void render();

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

	/// Opacity of 0 to 1
	float opacity;

// Positional
	/// All dimension stuffs are in unit of pixels
	float left() const;
	void setLeft(float val);

	float right() const;
	void setRight(float val);

	float top() const;
	void setTop(float val);

	float bottom() const;
	void setBottom(float val);

	unsigned width() const;
	void setWidth(unsigned val);

	unsigned height() const;
	void setHeight(unsigned val);

// Transform
	// http://www.w3.org/TR/css3-2d-transforms/
	// TODO: May move these transform into a transform component
	const Vec3& origin() const { return _origin; }
	const Mat44& localTransformation() const;
	Mat44 worldTransformation() const;

	void setIdentity();
	void setTransform(const char* transformStr);
	void setOrigin(const Vec3& v);

	Vec3 _origin;					/// In unit of percentage of the width and height
	mutable Mat44 _localToWorld;
	mutable Mat44 _localTransformation;

// Background
	float backgroundPositionX;
	float backgroundPositionY;
	bool setBackgroundPosition(const char* cssBackgroundPosition);

	bool setBackgroundImage(const char* cssUrl);
	Render::TexturePtr backgroundImage;

	bool setBackgroundColor(const char* cssColor);
	Color backgroundColor;

	static JSClass jsClass;
};	// ElementStyle

}	// namespace Dom

#endif	// __CSS_ELEMENTSTYLE_H__
