#ifndef __AUDIO_AUDIO_H__
#define __AUDIO_AUDIO_H__

#include "audiobuffer.h"

struct AudioLoader;
typedef struct AudioLoader AudioLoader;

struct AudioSound;
typedef struct AudioSound AudioSound;

void audioSound_destroy(AudioSound* sound);
const char* audioSound_getUri(AudioSound* sound);

struct AudioDevice;
typedef struct AudioDevice AudioDevice;

class ResourceManager;

// Context management
void audiodevice_init();
void audiodevice_close();
AudioDevice* audiodevice_create();
void audiodevice_destroy(AudioDevice* device);

// Create sound
AudioSound* audiodevice_createSound(AudioDevice* device, const char* uri, ResourceManager* resourceMgr);

// Work with sound
void audiodevice_playSound(AudioDevice* device, AudioSound* sound);
void audiodevice_pauseSound(AudioDevice* device, AudioSound* sound);
void audiodevice_stopSound(AudioDevice* device, AudioSound* sound);
void audiodevice_setSoundLoop(AudioDevice* device, AudioSound* sound, bool loop);
bool audiodevice_getSoundLoop(AudioDevice* device, AudioSound* sound);
void audiodevice_setSoundCurrentTime(AudioDevice* device, AudioSound* sound, float time);
float audiodevice_getSoundCurrentTime(AudioDevice* device, AudioSound* sound);
void audiodevice_setSoundvolume(AudioDevice* device, AudioSound* sound, float volume);
float audiodevice_getSoundvolume(AudioDevice* device, AudioSound* sound);

// Loading
typedef void* (*audiodevice_loadCallback)(void* userData, Audio::AudioBuffer* audioData, unsigned begin, unsigned end);	// Invoked when the audio device think it's time to load some data
Audio::AudioBuffer* audiodevice_load(AudioDevice* device, const char* uri, audiodevice_loadCallback callback, void* userData);

// Main loop iteration processing
void audiodevice_update(AudioDevice* device);

#endif	// __AUDIO_AUDIO_H__
