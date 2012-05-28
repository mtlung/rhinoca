#ifndef __XMLPARSER_H__
#define __XMLPARSER_H__

#include "../roar/base/roNonCopyable.h"

/*!	XmlParser is intended to be a high speed and easy-to-use XML Parser specialized for
	game development.

	Features:
	-It it fast as lighting and has very low memory usage.
	-Extremely easy to use.

	Limitations:
	-Forward-only, read-only.
	-The input xml file is not validated and assumed to be correct.

	The following code demonstrates the basic usage. A simple xml file like this is parsed:
	\code
	<?xml version="1.0"?>
	<config>
		<!-- This is a comment -->
		<model file="scene.3ds" type="mesh"/>
		<messageText caption="Hello world">
			Welcome to &quot;MCore 3D Engine&quot;.
		</messageText>
		<fragment>
		<![CDATA[
			main() {}
		]]>
		</fragment>
	</config>
	\endcode

	The code for parsing this file would look like this:
	\code
	// Get the xml document string
	std::wifstream ifs("test.xml");
	std::stringstream ss;
	ifs >> ss.rdbuf();

	XmlParser parser;
	parser.parse(const_cast<char*>(ss.str().c_str()));

	std::string messageText;
	std::string fragmentCode;
	std::string fileName;

	// Parse the file until end of document or any error occurred.
	bool ended = false;
	while(!ended) {
		XmlParser::Event::Enum e = parser.nextEvent();

		switch(e) {
		// In this xml file, the only text which occurs is the "messageText"
		case XmlParser::Event::Text:
			messageText = parser.textData();
			break;

		// In this xml file, the only CDATA which occurs is the "fragment"
		case XmlParser::Event::CData:
			fragmentCode = parser.textData();
			break;

		case XmlParser::Event::BeginElement:
			if(strcmp(parser.elementName(), "model") == 0)
				fileName = parser.attributeValue("file");
			break;

		case XmlParser::Event::Error:
		case XmlParser::Event::EndDocument:
			ended = true;
			break;

		default:
			break;
		}
	}
	\endcode
 */
class XmlParser : ro::NonCopyable
{
public:
	struct Event { enum Enum {
		BeginElement,	//!< An xml element, like <foo>
		Text,			//!< Text within an xml element: <foo>This is the text.</foo>
		Comment,		//!< An xml comment like <!-- I am a comment --> or a DTD definition.
		CData,			//!< An xml cdata section like <![CDATA[ this is some CDATA ]]>
		EndElement,		//!< End of an xml element, like </foo>
		EndDocument,	//!< End of the document.
		Unknown,		//!< Unknown element.
		Error			//!< Error occurred during parsing.
	}; };	// Event

	XmlParser();

	~XmlParser();

	/*!	Supply the xml document string to the parser.
		The parser will use and modify this string (by placing '\0' at appropriate position),
		all the element name, attribute name/value etc are returned as a pointer within this
		supplied string.

		The parser will not destroy the string, so you can use the pointer returned by elementName(),
		texData(), attributeName(), attributeValue() etc... until you destroy the string buffer.
	 */
	void parse(char* source);

	/*!	Parse the next xml node and return the type of this node.
		The xml document is presented as a series of \em Event, keep calling this function
		until \em EndDocument or \em Error event is encountered to parse the whole document.
	 */
	Event::Enum nextEvent();

	/*!	Get the name of the element.
		Valid when the current event is BeginElement or EndElement.
		\return "" if there is error.
	 */
	const char* elementName() const;

	//! Returns if an element is an empty element, like <foo />
	bool isEmptyElement() const;

	/*!	Get the text of a Text, Comment, CData or Unknown node.
		\return "" if there is error.
	 */
	const char* textData() const;

	//! Get the number of attrubute for the current element.
	size_t attributeCount() const;

	/*!	Get the name of the n-th attrubute for the current element.
		\return null if there is error.
	 */
	const char* attributeName(size_t idx) const;

	//@{
	/*!	Get the value of the n-th attrubute for the current element.
		\return null if there is error.
	 */
	const char* attributeValue(size_t idx) const;

	/*!	Get the value of attrubute with \em name for the current element.
		\return null if there is error.
	 */
	const char* attributeValue(const char* name) const;

	const char* attributeValue(const char* name, const char* defaultValue) const;

	const char* attributeValueIgnoreCase(const char* name) const;

	const char* attributeValueIgnoreCase(const char* name, const char* defaultValue) const;

	float attributeValueAsFloat(size_t idx, float defaultValue = 0.0f) const;

	float attributeValueAsFloat(const char* name, float defaultValue = 0.0f) const;

	float attributeValueAsFloatIgnoreCase(const char* name, float defaultValue = 0.0f) const;

	bool attributeValueAsBool(size_t idx, bool defaultValue = false) const;

	bool attributeValueAsBool(const char* name, bool defaultValue = false) const;

	bool attributeValueAsBoolIgnoreCase(const char* name, bool defaultValue = false) const;
	//@}

	/*!	In order to retrieve the attributes in an EndElement event,
		we need to call push during BeginElement and call pop in EndElement
	 */
	void pushAttributes();

	void popAttributes();

	/*!	A helper function to convert a string into float number.
		In case of any erro, the function will return the supplied defaultValue.
	 */
	static float stringToFloat(const char* str, float defaultValue = 0.0f);

	static bool stringToBool(const char* str, bool defaultValue = false);

protected:
	class Impl;
	Impl& mImpl;
};	// XmlParser

#endif	// __XMLPARSER_H__
