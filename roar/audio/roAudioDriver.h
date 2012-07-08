#ifndef __audio_roAudioDriver_h__
#define __audio_roAudioDriver_h__

#include "../platform/roCompiler.h"

#ifdef __cplusplus
extern "C" {
#endif

struct roADriverListener;

struct roADriverSoundSource;

typedef struct roAudioDriver
{
// Listener
	roADriverListener* (*newListener)();
	void (*deleteListener)(roADriverListener* self);

// Sound source
	roADriverSoundSource* (*newSoundSource)(roAudioDriver* self, const roUtf8* uri, const roUtf8* typeHint, bool streaming);
	void (*deleteSoundSource)(roADriverSoundSource* self, bool delayTillPlaybackFinish);
	void (*preloadSoundSource)(roADriverSoundSource* self);

	void (*playSoundSource)(roADriverSoundSource* self);	/// Implies rewind
	bool (*soundSourceIsPlaying)(roADriverSoundSource* self);
	void (*stopSoundSource)(roADriverSoundSource* self);
	void (*soundSourceStopAll)(roAudioDriver* self);

	bool (*soundSourceGetLoop)(roADriverSoundSource* self);
	void (*soundSourceSetLoop)(roADriverSoundSource* self, bool loop);

	void (*soundSourceSetPause)(roADriverSoundSource* self, bool pause);

	float (*soundSourceTellPos)(roADriverSoundSource* self);
	void (*soundSourceSeekPos)(roADriverSoundSource* self, float time);

	bool (*soundSourceReady)(roADriverSoundSource* self);
	bool (*soundSourceAborted)(roADriverSoundSource* self);
	bool (*soundSourceFullyLoaded)(roADriverSoundSource* self);

	void (*setSoundSourcePriority)(roADriverSoundSource* self, roSize priority);

// Others
	void (*setMaxPlayingSound)(roSize count);

	void (*tick)(roAudioDriver* self);
} roAudioDriver;

roAudioDriver* roNewAudioDriver();
bool roInitAudioDriver(roAudioDriver* self, const char* options);
void roDeleteAudioDriver(roAudioDriver* self);

#ifdef __cplusplus
}
#endif

#endif	// __audio_roAudioDriver_h__
