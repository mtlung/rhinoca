#ifndef __DOME_ELEMENT_H__
#define __DOME_ELEMENT_H__

#include "node.h"

namespace Dom {

// Reference: http://www.w3schools.com/dom/dom_element.asp
class Element : public Node
{
public:
// Attributes
	FixString id;

	static JSClass jsClass;
};	// Element

class ElementFactory
{
public:
	ElementFactory();
	~ElementFactory();

	static ElementFactory& singleton();

	/// Returns a new Element if elementType can be found, null otherwise
	Element* create(const char* type);

	/// A function that return a new Element if the elementType matched, otherwise return null
	typedef Element* (*FactoryFunc)(const char* type);
	void addFactory(FactoryFunc factory);

protected:
	FactoryFunc* _factories;
	int _factoryCount;
	int _factoryBufCount;
};	// ElementFactory

}	// namespace Dom

#endif	// __DOME_ELEMENT_H__
