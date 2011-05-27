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
	override void bind(JSContext* cx, JSObject* parent);

	static void registerClass(JSContext* cx, JSObject* parent);
	static Element* factoryCreate(Rhinoca* rh, const char* type, XmlParser* parser);

	override void play();
	override void pause();

// Attributes
	override const FixString& tagName() const;

	static JSClass jsClass;

	override const char* src() const;
	override void setSrc(const char* uri);

	override bool loop() const;
	override void setLoop(bool loop);

	AudioSound* _sound;
	AudioDevice* _device;
	ResourceManager* _resourceManager;
};	// HTMLAudioElement

}	// namespace Dom

#endif	// __DOM_AUDIO_H__
