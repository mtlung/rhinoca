#ifndef __DOME_ELEMENT_H__
#define __DOME_ELEMENT_H__

#include "node.h"

class Path;
class NodeList;
class XmlParser;

namespace Dom {

// Reference: http://www.w3schools.com/dom/dom_element.asp
class Element : public Node
{
public:
	Element();

// Operations
	JSObject* createPrototype();

	static void registerClass(JSContext* cx, JSObject* parent);

	NodeList* getElementsByTagName(const char* tagName);

	/// Helper function to convert a uri into a usable path for ResourceManager
	void fixRelativePath(const char* uri, const char* docUri, Path& path);

// Attributes
	FixString id;
	bool visible;
	rhint32 top, left;

	override const FixString& tagName() const;

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

class ElementStyle : public JsBindable
{
public:
// Operations
	void bind(JSContext* cx, JSObject* parent);

// Attributes
	Element* element;

	static JSClass jsClass;
};	// ElementStyle

}	// namespace Dom

#endif	// __DOME_ELEMENT_H__
