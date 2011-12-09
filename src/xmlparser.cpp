#include "pch.h"
#include "xmlparser.h"
#include "common.h"
#include <memory.h>	// For memcpy
#include "stack.h"
#include "../roar/base/roArray.h"
#include "../roar/base/roString.h"

// Adopted from Irrlicht engine's xml parser
// http://www.ambiera.com/irrxml/

//! returns true if a character is whitespace
static inline bool isWhiteSpace(char c)
{
	return (c == ' ' || c == '\t' || c == '\n' || c == '\r');
}

static const char* createString(const char* begin, char* end)
{
	*end = '\0';
	return begin;
}

// Returns true if both string are equals.
// The string lhs should be null terminated while rhs need not to be.
static bool MyStrCmpLhsFixed(const char* lhs, const char* rhs)
{
	for(;*lhs != '\0'; ++lhs, ++rhs)
		if(*lhs != *rhs)
			return false;
	return true;
}

static const char* escapeString(char* begin, char* end)
{
	// Characters that need to escape
	// Reference: http://www.w3schools.com/xml/xml_cdata.asp
	static const char cEscapeChar[5] = {'&', '<', '>', '\'', '\"'};
	static const char* cEscapeString[5] = {"&amp;", "&lt;", "&gt;", "&apos;", "&quot;"};
	static const size_t cEscapeStringLen[5] = {5, 4, 4, 6, 6};

	char* str = begin;

	// Scan for '&' character
	for(; str != end; ++str) {
		if(*str != '&')
			continue;

		// Loop for all escape string
		for(size_t i = 0; i<5; ++i) {
			if(!MyStrCmpLhsFixed(cEscapeString[i], str))
				continue;

			// Replace with cEscapeChar
			*str = cEscapeChar[i];
			// Move the chars to the front
			const char* trailPos = str + cEscapeStringLen[i];
			::memcpy(str+1, trailPos, end - trailPos);
			// Adjust the new end
			end -= (cEscapeStringLen[i] - 1);
			break;
		}
	}

	*end = '\0';

	return begin;
}

class XmlParser::Impl
{
	typedef XmlParser::Event Event;

	struct Attribute {
		const char* name;
		const char* value;
	};	// Attribute

	typedef ro::Array<Attribute> Attributes;

public:
	Impl() : mIsEmptyElement(false)
	{
		parse(NULL);
	}

	void parse(char* source)
	{
		p = source;
		mElementName = "";
		mText = "";
		mHasBackupOpenTag = false;
		mCurrentNodeType = Event::Error;
		mAttributes.clear();
	}

	Event::Enum nextEvent()
	{
		mAttributes.clear();

		// Gives an end element event if the last element close directly
		if(mIsEmptyElement) {
			mIsEmptyElement = false;
			return Event::EndElement;
		}

		if(!p)
			return mCurrentNodeType = Event::Error;

		char* start = p;
		char* backup = p;

		// NOTE: Special care for the <script> tag, which it's text element may contains '<' and '>' characters
		// Move until </script> found
		if(mCurrentNodeType == Event::BeginElement && strcmp(mElementName, "script") == 0) {
			char tmp[] = "</script>";
			size_t tmpIdx = 0;
			while(true)	{
				while(*p && *p == tmp[tmpIdx]) {
					++p;
					++tmpIdx;
					if(tmp[tmpIdx] == '\0')
						goto Finish;
				}
				++p;
				tmpIdx = 0;
			}
			Finish:
			p -= sizeof(tmp) - 1;	// -1 for the '\0' character
			;
		}
		// Move forward until '<' found
		else if(!mHasBackupOpenTag) while(*p != '<' && *p)
			++p;
		else
			*p = '<';

		if(!*p)
			return mCurrentNodeType = Event::EndDocument;

		if(p - start > 0) {
			// We found some text, store it
			if(setText(start, p))
				return mCurrentNodeType;
		}

		++p;

		// Based on current token, parse and report next element
		switch(*p) {
		case '/':
			parseClosingXMLElement();
			break;
		case '?':
			ignoreDefinition();
			break;
		case '!':
			if(!parseCDATA() && !parseComment())
				ignoreDefinition();
			break;
		default:
			parseOpeningXMLElement();
			break;
		}

		if(mHasBackupOpenTag) {
			*backup = '\0';
			mHasBackupOpenTag = false;
		}

		return mCurrentNodeType;
	}

	bool setText(char* start, char* end)
	{
		// Check if text is more than 2 characters, and if not, check if there is
		// only white space, so that this text won't be reported
		if(end - start < 3) {
			const char* q = start;
			for(; q != end; ++q)
				if(!isWhiteSpace(*q))
					break;

			if(q == end)
				return false;
		}

		// Since we may lost the '<' character, we use a special tag to remember it
		mHasBackupOpenTag = (*end == '<');

		// Set current text to the parsed text, and replace xml special characters
		mText = escapeString(start, end);

		// Current XML node type is text
		mCurrentNodeType = Event::Text;

		return true;
	}

	//! Ignores an xml definition like <?xml something />
	void ignoreDefinition()
	{
		mCurrentNodeType = Event::Unknown;

		// Move until end marked with '>' reached
		while(*p && *p != '>')
			++p;
		++p;
	}

	//! parses a comment
	bool parseComment()
	{
		if(*(p+1) != '-' || *(p+2) != '-')
			return false;

		mCurrentNodeType = Event::Comment;

		// Skip '<!--'
		p += 3;

		const char* pCommentBegin = p;
		char* pCommentEnd = NULL;

		// Find end of comment "--"
		while(*p && !pCommentEnd) {
			if(*p == '>' && (*(p-1) == '-') && (*(p-2) == '-'))
				pCommentEnd = p - 2;
			++p;
		}

		if(pCommentEnd)
			mText = createString(pCommentBegin, pCommentEnd);
		else
			mText = "";

		return true;
	}

	//! Parses an opening xml element and reads attributes
	void parseOpeningXMLElement()
	{
		mCurrentNodeType = Event::BeginElement;
		mIsEmptyElement = false;
		mText = "";

		// Find name
		const char* startName = p;

		// find end of element
		while(*p != '>' && !isWhiteSpace(*p))
			++p;

		char* endName = p;

		// find mAttributes
		while(*p != '>')
		{
			if(isWhiteSpace(*p))
				++p;
			else
			{
				if(*p != '/')
				{
					// We've got an attribute

					// Read the attribute names
					const char* attributeNameBegin = p;

					while(!isWhiteSpace(*p) && *p != '=')
						++p;

					char* attributeNameEnd = p;
					++p;

					// Read the attribute value, check for quotes and single quotes
					while((*p != '\"') && (*p != '\'') && *p)
						++p;

					if(!*p) // Malformatted xml file
						return;

					const char attributeQuoteChar = *p;

					++p;
					char* attributeValueBegin = p;

					while(*p != attributeQuoteChar && *p)
						++p;

					if(!*p) // Malformatted xml file
						return;

					char* attributeValueEnd = p;
					++p;

					Attribute attr;
					attr.name = createString(attributeNameBegin, attributeNameEnd);
					attr.value = escapeString(attributeValueBegin, attributeValueEnd);
					mAttributes.pushBack(attr);
				}
				else
				{
					// Tag is closed directly
					++p;
					mIsEmptyElement = true;
					break;
				}
			}
		}

		// Check if this tag is closing directly
		if(endName > startName && *(endName-1) == '/')
		{
			// Directly closing tag
			mIsEmptyElement = true;
			--endName;
		}

		mElementName = createString(startName, endName);

		++p;
	}

	//! Parses an closing xml tag
	void parseClosingXMLElement()
	{
		mCurrentNodeType = Event::EndElement;
		mIsEmptyElement = false;

		++p;
		char* pBeginClose = p;

		while(*p && *p != '>')
			++p;

		if(!*p) {
			mCurrentNodeType = Event::Error;
			return;
		}

		mElementName = createString(pBeginClose, p);
		++p;
	}

	//! Parses a possible CDATA section, returns false if begin was not a CDATA section
	bool parseCDATA()
	{
		if(*(p+1) != '[')
			return false;

		mCurrentNodeType = Event::CData;

		// Skip '<![CDATA['
		int count=0;
		while(*p && count < 8) {
			++p;
			++count;
		}

		if(!*p)
			return true;

		const char* cDataBegin = p;
		char* cDataEnd = NULL;

		// Find end of CDATA
		while(*p && !cDataEnd) {
			if(*p == '>' && (*(p-1) == ']') && (*(p-2) == ']'))
				cDataEnd = p - 2;
			++p;
		}

		if(cDataEnd)
			mText = createString(cDataBegin, cDataEnd);
		else
			mText = "";

		return true;
	}

	const char* attributeValue(const char* name) const
	{
		if(!name)
			return NULL;

		for(Attributes::const_iterator i=mAttributes.begin(); i!=mAttributes.end(); ++i)
			if(::strcmp(name, i->name) == 0)
				return i->value;
		return NULL;
	}

	const char* attributeValueIgnoreCase(const char* name) const
	{
		if(!name)
			return NULL;

		for(Attributes::const_iterator i=mAttributes.begin(); i!=mAttributes.end(); ++i)
			if(roStrCaseCmp(name, i->name) == 0)
				return i->value;
		return NULL;
	}

	void pushAttributes()
	{
		mStack.push(mAttributes);
	}

	void popAttributes()
	{
		mAttributes = mStack.top();
		mStack.pop();
	}

	char* p;	//!< The current pointer
	const char* mText;
	const char* mElementName;
	bool mIsEmptyElement;
	bool mHasBackupOpenTag;	//! Set to true when '\0' is assigned because of Text event.
	Event::Enum mCurrentNodeType;

	Attributes mAttributes;

	Stack<Attributes> mStack;
};	// Impl

XmlParser::XmlParser()
	: mImpl(*new Impl)
{
}

XmlParser::~XmlParser()
{
	delete &mImpl;
}

void XmlParser::parse(char* source)
{
	mImpl.parse(source);
}

XmlParser::Event::Enum XmlParser::nextEvent()
{
	return mImpl.nextEvent();
}

const char* XmlParser::elementName() const
{
	return mImpl.mElementName;
}

bool XmlParser::isEmptyElement() const
{
	return mImpl.mIsEmptyElement;
}

const char* XmlParser::textData() const
{
	return mImpl.mText;
}

size_t XmlParser::attributeCount() const
{
	return mImpl.mAttributes.size();
}

const char* XmlParser::attributeName(size_t idx) const
{
	return idx < mImpl.mAttributes.size() ? mImpl.mAttributes[idx].name : NULL;
}

const char* XmlParser::attributeValue(size_t idx) const
{
	return idx < mImpl.mAttributes.size() ? mImpl.mAttributes[idx].value : NULL;
}

const char* XmlParser::attributeValue(const char* name) const
{
	return mImpl.attributeValue(name);
}

const char* XmlParser::attributeValue(const char* name, const char* defaultValue) const
{
	if(const char* ret = mImpl.attributeValue(name))
		return ret;
	return defaultValue;
}

const char* XmlParser::attributeValueIgnoreCase(const char* name) const
{
	return mImpl.attributeValueIgnoreCase(name);
}

const char* XmlParser::attributeValueIgnoreCase(const char* name, const char* defaultValue) const
{
	if(const char* ret = mImpl.attributeValueIgnoreCase(name))
		return ret;
	return defaultValue;
}

float XmlParser::attributeValueAsFloat(size_t idx, float defaultValue) const
{
	return stringToFloat(attributeValue(idx), defaultValue);
}

float XmlParser::attributeValueAsFloat(const char* name, float defaultValue) const
{
	return stringToFloat(attributeValue(name), defaultValue);
}

float XmlParser::attributeValueAsFloatIgnoreCase(const char* name, float defaultValue) const
{
	return stringToFloat(attributeValueIgnoreCase(name), defaultValue);
}

bool XmlParser::attributeValueAsBool(size_t idx, bool defaultValue) const
{
	return stringToBool(attributeValue(idx), defaultValue);
}

bool XmlParser::attributeValueAsBool(const char* name, bool defaultValue) const
{
	return stringToBool(attributeValue(name), defaultValue);
}

bool XmlParser::attributeValueAsBoolIgnoreCase(const char* name, bool defaultValue) const
{
	return stringToBool(attributeValueIgnoreCase(name), defaultValue);
}

void XmlParser::pushAttributes()
{
	return mImpl.pushAttributes();
}

void XmlParser::popAttributes()
{
	return mImpl.popAttributes();
}

float XmlParser::stringToFloat(const char* str, float defaultValue)
{
	return str ? roStrToFloat(str, defaultValue) : defaultValue;
}

bool XmlParser::stringToBool(const char* str, bool defaultValue)
{
	return str ? roStrToBool(str, defaultValue) : defaultValue;
}
