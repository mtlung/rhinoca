#ifndef __DOM_MEDIA_H__
#define __DOM_MEDIA_H__

#include "element.h"

namespace Dom {

/// See: http://dev.w3.org/html5/spec/Overview.html#media-element
class HTMLMediaElement : public Element
{
public:
	HTMLMediaElement();
	~HTMLMediaElement();

// Operations
	JSObject* createPrototype();

	static void registerClass(JSContext* cx, JSObject* parent);

	virtual void setSrc(const char* uri) {}
	virtual const char* getSrc() { return ""; }

	void load();
	virtual void play() {}
	virtual void pause() {}

	void parseMediaElementAttributes(Rhinoca* rh, XmlParser* parser);

// Attributes
	FixString src;

	/// type:	audio/mpeg
	///			audio/ogg; codecs='vorbis'
	///			audio/x-wav
	/// Returns: "" or "probably" or "maybe"
	virtual const char* canPlayType(const char* type) const { return ""; }

	/// HAVE_NOTHING = 0
	/// HAVE_METADATA = 1
	/// HAVE_CURRENT_DATA = 2
	/// HAVE_FUTURE_DATA = 3
	/// HAVE_ENOUGH_DATA = 4
	virtual int readyState() const { return 0; }

	virtual double currentTime() const { return 0; }
	virtual void setCurrentTime(double) {}

	virtual bool autoplay() const { return true; }
	virtual void setAutoplay(bool) {}

	virtual bool loop() const { return false; }
	virtual void setLoop(bool) {}

	static JSClass jsClass;
};	// HTMLImageElement

}	// namespace Dom

#endif	// __DOM_MEDIA_H__
