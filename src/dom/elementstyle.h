#ifndef __CSS_ELEMENTSTYLE_H__
#define __CSS_ELEMENTSTYLE_H__

#include "../jsbindable.h"
#include "../../roar/math/roMatrix.h"
#include "../../roar/render/roColor.h"
#include "../../roar/render/roTexture.h"

namespace Dom {

class Element;
class CanvasRenderingContext2D;

class ElementStyle : public JsBindable
{
public:
	explicit ElementStyle(Dom::Element* ele);
	~ElementStyle();

// Operations
	/// Calculate it's _localToWorld matrix
	/// For this function to work, you must call this function for an Element's parent first
	void updateTransformation();

	void render(CanvasRenderingContext2D* virtualCanvas);

	void bind(JSContext* cx, JSObject* parent) override;

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
	ro::Vec3 origin() const;		/// Always in unit of pixel
	const ro::Mat4& localTransformation() const;
	ro::Mat4 worldTransformation() const;

	void setIdentity();
	void setTransform(const char* transformStr);
	void setTransformOrigin(const char* transformOriginStr);

	ro::Vec3 _origin;				/// Transformation origin, the unit can be pixel or percentage
	bool _originUsePercentage[4];	/// Indicate if _origin is in unit of percentage

	mutable ro::Mat4 _localToWorld;
	mutable ro::Mat4 _localTransformation;

// Background
	float backgroundPositionX;
	float backgroundPositionY;
	bool setBackgroundPosition(const char* cssBackgroundPosition);

	bool setBackgroundImage(const char* cssUrl);
	ro::TexturePtr backgroundImage;

	bool setBackgroundColor(const char* cssColor);
	ro::Colorf backgroundColor;

	static JSClass jsClass;
};	// ElementStyle

}	// namespace Dom

#endif	// __CSS_ELEMENTSTYLE_H__
