#ifndef __DOM_AUDIO_H__
#define __DOM_AUDIO_H__

#include "media.h"

struct AudioSound;
struct AudioDevice;

namespace ro {
struct ResourceManager;
}	// namespace ro

namespace Dom {

class HTMLAudioElement : public HTMLMediaElement
{
public:
	HTMLAudioElement(Rhinoca* rh, AudioDevice* device, ro::ResourceManager* mgr);
	~HTMLAudioElement();

// Operations
	override void bind(JSContext* cx, JSObject* parent);

	static void registerClass(JSContext* cx, JSObject* parent);
	static Element* factoryCreate(Rhinoca* rh, const char* type, XmlParser* parser);

	override void play();
	override void pause();

	override Node* cloneNode(bool recursive);

// Attributes
	override const ro::ConstString& tagName() const;

	static JSClass jsClass;

	override const char* src() const;
	override void setSrc(const char* uri);

	override double currentTime() const;
	override void setCurrentTime(double time);

	override bool loop() const;
	override void setLoop(bool loop);

	AudioSound* _sound;
	AudioDevice* _device;
	ro::ResourceManager* _resourceManager;
};	// HTMLAudioElement

}	// namespace Dom

#endif	// __DOM_AUDIO_H__
