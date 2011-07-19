#ifndef __DOM_ELEMENT_H__
#define __DOM_ELEMENT_H__

#include "node.h"

class Path;
class NodeList;
class XmlParser;

namespace Dom {

class ElementStyle;

// Reference: http://www.w3schools.com/dom/dom_element.asp
class Element : public Node
{
public:
	explicit Element(Rhinoca* rh);

// Operations
	JSObject* createPrototype();

	static void registerClass(JSContext* cx, JSObject* parent);

	NodeList* getElementsByTagName(const char* tagName);

	/// Helper function to convert a uri into a usable path for ResourceManager
	void fixRelativePath(const char* uri, const char* docUri, Path& path);

	override void render();

// Attributes
	FixString id;
	bool visible;

	int _top, _left;
	int _width, _height;	// NOTE: It's intentional to type _width and _height as signed integer

	override const FixString& tagName() const;

	virtual int top() const { return _top; }
	virtual int left() const { return _left; }

	virtual unsigned width() const { return _width; }
	virtual unsigned height() const { return _height; }

	virtual void setWidth(unsigned w) { _width = w; }
	virtual void setheight(unsigned h) { _height = h; }

	int right() const { return left() + width(); }
	int bottom() const { return top() + height(); }

	ElementStyle* style;

	static JSClass jsClass;
};	// Element

class ElementFactory
{
public:
	ElementFactory();
	~ElementFactory();

	static ElementFactory& singleton();

	/// Returns a new Element if elementType can be found, null otherwise
	/// Optional XmlParser to set the Element's attributes
	Element* create(Rhinoca* rh, const char* type, XmlParser* parser=NULL);

	/// A function that return a new Element if the elementType matched, otherwise return null
	typedef Element* (*FactoryFunc)(Rhinoca* rh, const char* type, XmlParser* parser);
	void addFactory(FactoryFunc factory);

protected:
	FactoryFunc* _factories;
	int _factoryCount;
	int _factoryBufCount;
};	// ElementFactory

}	// namespace Dom

#endif	// __DOM_ELEMENT_H__
