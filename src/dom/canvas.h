#ifndef __DOM_CANVAS_H__
#define __DOM_CANVAS_H__

#include "element.h"
#include "../render/framebuffer.h"

namespace Render { class Texture; }

namespace Dom {

class HTMLCanvasElement : public Element
{
public:
	HTMLCanvasElement();
	~HTMLCanvasElement();

// Operations
	void bind(JSContext* cx, JSObject* parent);

	void bindFramebuffer();
	void unbindFramebuffer();

	void useExternalFrameBuffer(Rhinoca* rh);

	void createContext(const char* ctxName);

	virtual void render();

	static void registerClass(JSContext* cx, JSObject* parent);
	static Element* factoryCreate(Rhinoca* rh, const char* type, XmlParser* parser);

// Attributes
	class Context : public JsBindable
	{
	public:
		explicit Context(HTMLCanvasElement*);
		virtual ~Context();
		HTMLCanvasElement* canvas;
	};	// Context

	Context* context;

	Render::Texture* texture();

	int width() const { return _framebuffer.width; }
	int height() const { return _framebuffer.height; }

	void setWidth(unsigned width);
	void setHeight(unsigned height);

	virtual const char* tagName() const;

	static JSClass jsClass;

protected:
	Render::Framebuffer _framebuffer;
};	// HTMLCanvasElement

}	// namespace Dom

#endif	// __DOM_CANVAS_H__
