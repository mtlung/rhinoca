#ifndef __DOM_CANVAS_H__
#define __DOM_CANVAS_H__

#include "element.h"
#include "../render/framebuffer.h"

namespace Render { class Texture; }

namespace Dom {

class Canvas : public Element
{
public:
	Canvas();
	~Canvas();

// Operations
	void bind(JSContext* cx, JSObject* parent);

	void bindFramebuffer();
	void unbindFramebuffer();

	static Element* factoryCreate(Rhinoca* rh, const char* type, XmlParser* parser);

// Attributes
	class Context : public JsBindable
	{
	public:
		explicit Context(Canvas*);
		virtual ~Context();
		Canvas* canvas;
	};	// Context

	Context* context;

	Render::Texture* texture();

	int width() const { return _framebuffer.width; }
	int height() const { return _framebuffer.height; }

	void setWidth(unsigned width);
	void setHeight(unsigned height);

	static JSClass jsClass;

protected:
	Render::Framebuffer _framebuffer;
};	// Canvas

}	// namespace Dom

#endif	// __DOM_CANVAS_H__
