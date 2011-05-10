#ifndef __DOM_AUDIO_H__
#define __DOM_AUDIO_H__

#include "media.h"

namespace Dom {

class HTMLAudioElement : public HTMLMediaElement
{
public:
	HTMLAudioElement();
	~HTMLAudioElement();

// Operations
	void bind(JSContext* cx, JSObject* parent);

	static void registerClass(JSContext* cx, JSObject* parent);
	static Element* factoryCreate(Rhinoca* rh, const char* type, XmlParser* parser);

// Attributes
	virtual const char* tagName() const;

	static JSClass jsClass;
};	// HTMLAudioElement

}	// namespace Dom

#endif	// __DOM_AUDIO_H__
