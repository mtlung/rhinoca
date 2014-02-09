#ifndef __DOM_AUDIO_H__
#define __DOM_AUDIO_H__

#include "media.h"

struct roADriverSoundSource;

namespace ro {
struct ResourceManager;
}	// namespace ro

namespace Dom {

class HTMLAudioElement : public HTMLMediaElement
{
public:
	HTMLAudioElement(Rhinoca* rh);
	~HTMLAudioElement();

// Operations
	void bind(JSContext* cx, JSObject* parent) override;

	static void registerClass(JSContext* cx, JSObject* parent);
	static Element* factoryCreate(Rhinoca* rh, const char* type, XmlParser* parser);

	void play() override;
	void pause() override;

	Node* cloneNode(bool recursive) override;

	void tick(float dt) override;

// Attributes
	const ro::ConstString& tagName() const override;

	static JSClass jsClass;

	const char* src() const override;
	void setSrc(const char* uri) override;

	double currentTime() const override;
	void setCurrentTime(double time) override;

	bool loop() const override;
	void setLoop(bool loop) override;

	roADriverSoundSource* _sound;
};	// HTMLAudioElement

}	// namespace Dom

#endif	// __DOM_AUDIO_H__
