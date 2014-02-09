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
	void bind(JSContext* cx, JSObject* parent) override;

	void createContext(const char* ctxName);

	void render(CanvasRenderingContext2D* virtualCanvas) override;

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
		virtual ro::Texture* texture() = 0;
		HTMLCanvasElement* canvas;
	};	// Context

	Context* context;

	ro::Texture* texture();

	unsigned width() const override { return _width; }
	unsigned height() const override { return _height; }

	void setWidth(unsigned width) override;
	void setHeight(unsigned height) override;

	const ro::ConstString& tagName() const override;

	static JSClass jsClass;

	bool _useRenderTarget;
	unsigned _width, _height;
};	// HTMLCanvasElement

}	// namespace Dom

#endif	// __DOM_CANVAS_H__
