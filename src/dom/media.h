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

	void load();
	virtual void play() {}
	virtual void pause() {}

	void parseMediaElementAttributes(Rhinoca* rh, XmlParser* parser);

// Attributes
	enum MediaError {
		MEDIA_ERR_ABORTED			= 1,
		MEDIA_ERR_NETWORK			= 2,
		MEDIA_ERR_DECODE			= 3,
		MEDIA_ERR_SRC_NOT_SUPPORTED	= 4
	};

	enum NetworkState {
		/// The element has not yet been initialized. All attributes are in their initial states.
		NETWORK_EMPTY		= 0,

		/// The element's resource selection algorithm is active and has selected a resource,
		/// but it is not actually using the network at this time.
		NETWORK_IDLE		= 1,

		/// The user agent is actively trying to download data.
		NETWORK_LOADING		= 2,

		/// The element's resource selection algorithm is active,
		/// but it has failed to find a resource to use.
		NETWORK_NO_SOURCE	= 3
	} networkState;

	enum ReadyState {
		/// No information regarding the media resource
		HAVE_NOTHING		= 0,

		/// Enough for getting some info like duration, but no data for playback
		HAVE_METADATA		= 1,

		/// Data for the immediate current playback positionp is available.
		/// For example, a video can display the current frame but cannot play to next frame
		HAVE_CURRENT_DATA	= 2,

		/// Data for the immediate current playback position is available,
		/// as well as enough data for the user agent to advance the current
		/// playback position in the direction of playback at least a little
		/// without immediately reverting to the HAVE_METADATA state.
		HAVE_FUTURE_DATA	= 3,

		/// All the conditions described for the HAVE_FUTURE_DATA state are met,
		/// and, in addition, the user agent estimates that data is being fetched
		/// at a rate where the media can play smoothly with defaultPlaybackRate.
		HAVE_ENOUGH_DATA	= 4
	} readyState;

	/// type:	audio/mpeg
	///			audio/ogg; codecs='vorbis'
	///			audio/x-wav
	/// Returns: "" or "probably" or "maybe"
	virtual const char* canPlayType(const char* type) const { return ""; }

	virtual const char* src() const { return ""; }
	virtual void setSrc(const char* uri) {}

	virtual double currentTime() const { return 0; }
	virtual void setCurrentTime(double) {}

	virtual double startTime() const { return 0; }

	virtual double duration() const { return 0; }

	virtual bool paused() const { return false; }

	virtual bool ended() const { return false; }

	virtual bool autoplay() const { return true; }
	virtual void setAutoplay(bool) {}

	virtual bool loop() const { return false; }
	virtual void setLoop(bool) {}

	static JSClass jsClass;

protected:
	FixString _src;
};	// HTMLImageElement

}	// namespace Dom

#endif	// __DOM_MEDIA_H__
