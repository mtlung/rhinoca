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
	void (*deleteSoundSource)(roADriverSoundSource* self);
	void (*preloadSoundSource)(roADriverSoundSource* self);

	void (*playSoundSource)(roADriverSoundSource* self);	/// Implies rewind
	bool (*soundSourceIsPlaying)(roADriverSoundSource* self);
	void (*pauseSoundSource)(roADriverSoundSource* self);
	void (*resumeSoundSource)(roADriverSoundSource* self);
	void (*stopSoundSource)(roADriverSoundSource* self);

	float (*soundSourceTellPos)(roADriverSoundSource* self);
	void (*soundSourceSeekPos)(roADriverSoundSource* self, float time);

	void (*setSoundSourcePriority)(roADriverSoundSource* self, roSize priority);

// Feeding custom audio data (they are thread safe)
/*	bool (*isRequestQueueEmpty)();
	void (*popRequestQueue)(roUint64* id, roUtf8** uri);
	void (*processSoundData)(roUint64 id, const roBytePtr data, roSize bytes);*/

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
