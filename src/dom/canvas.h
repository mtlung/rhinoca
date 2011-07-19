#ifndef __DOM_CANVAS_H__
#define __DOM_CANVAS_H__

#include "element.h"
#include "../render/framebuffer.h"

namespace Render { class Texture; }

namespace Dom {

class HTMLCanvasElement : public Element
{
public:
	explicit HTMLCanvasElement(Rhinoca* rh);
	HTMLCanvasElement(Rhinoca* rh, unsigned width, unsigned height, bool frontBufferOnly);
	~HTMLCanvasElement();

// Operations
	override void bind(JSContext* cx, JSObject* parent);

	void bindFramebuffer();

	void createTextureFrameBuffer(unsigned w, unsigned h);
	void useExternalFrameBuffer(Rhinoca* rh);

	void createContext(const char* ctxName);

	override void render();

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

	override unsigned width() const { return _framebuffer.width; }
	override unsigned height() const { return _framebuffer.height; }

	override void setWidth(unsigned width);
	override void setHeight(unsigned height);

	override const FixString& tagName() const;

	static JSClass jsClass;

protected:
	Render::Framebuffer _framebuffer;
};	// HTMLCanvasElement

}	// namespace Dom

#endif	// __DOM_CANVAS_H__
