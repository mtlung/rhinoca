#ifndef __DOME_ELEMENT_H__
#define __DOME_ELEMENT_H__

#include "node.h"

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

// Attributes
	FixString id;
	bool visible;
	long top, left;

	virtual const char* tagName() const;

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
