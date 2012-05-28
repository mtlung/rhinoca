#ifndef __DOM_CANVAS_H__
#define __DOM_CANVAS_H__

#include "element.h"
#include "../../roar/render/roTexture.h"

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

	void createContext(const char* ctxName);

	override void render(CanvasRenderingContext2D* virtualCanvas);

	static void registerClass(JSContext* cx, JSObject* parent);
	static Element* factoryCreate(Rhinoca* rh, const char* type, XmlParser* parser);

// Attributes
	class Context : public JsBindable
	{
	public:
		explicit Context(HTMLCanvasElement*);
		virtual ~Context();
		virtual unsigned width() const = 0;
		virtual unsigned height() const = 0;
		virtual void setWidth(unsigned width) = 0;
		virtual void setHeight(unsigned height) = 0;
		HTMLCanvasElement* canvas;
	};	// Context

	Context* context;

	override unsigned width() const { return _width; }
	override unsigned height() const { return _height; }

	override void setWidth(unsigned width);
	override void setHeight(unsigned height);

	override const ro::ConstString& tagName() const;

	static JSClass jsClass;

	bool _useRenderTarget;
	unsigned _width, _height;
};	// HTMLCanvasElement

}	// namespace Dom

#endif	// __DOM_CANVAS_H__
