#ifndef __DOM_AUDIO_H__
#define __DOM_AUDIO_H__

#include "media.h"
#include "../audio/audiodevice.h"

namespace Dom {

class HTMLAudioElement : public HTMLMediaElement
{
public:
	HTMLAudioElement(AudioDevice* device, ResourceManager* mgr);
	~HTMLAudioElement();

// Operations
	void bind(JSContext* cx, JSObject* parent);

	static void registerClass(JSContext* cx, JSObject* parent);
	static Element* factoryCreate(Rhinoca* rh, const char* type, XmlParser* parser);

	virtual void setSrc(const char* uri);
	virtual const char* getSrc();

	virtual void play();
	virtual void pause();

// Attributes
	virtual const char* tagName() const;

	static JSClass jsClass;

	void setLoop(bool loop);
	bool getLoop();

	AudioSound* _sound;
	AudioDevice* _device;
	ResourceManager* _resourceManager;
};	// HTMLAudioElement

}	// namespace Dom

#endif	// __DOM_AUDIO_H__
