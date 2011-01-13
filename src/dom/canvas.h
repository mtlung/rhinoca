#ifndef __DOME_CANVAS_H__
#define __DOME_CANVAS_H__

#include "element.h"
#include "../render/framebuffer.h"

namespace Dom {

class Canvas : public Element
{
public:
	Canvas();
	~Canvas();

// Operations
	void bind(JSContext* cx, JSObject* parent);

	static Element* factoryCreate(const char* type);

// Attributes
	class Context : public JsBindable
	{
	public:
		explicit Context(Canvas*);
		virtual ~Context();
		Canvas* canvas;
	};	// Context

	Context* context;

	int width() const { return _width; }
	int height() const { return _height; }

	void setWidth();
	void setHeight();

	static JSClass jsClass;

protected:
	int _width;
	int _height;
	Render::Framebuffer _framebuffer;
};	// Canvas

}	// namespace Dom

#endif	// __DOME_CANVAS_H__
