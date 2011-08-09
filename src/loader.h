#ifndef __LOADER_LOADER_H__
#define __LOADER_LOADER_H__

#include "rhinoca.h"

class Resource;
class ResourceManager;

namespace Loader {

#ifdef RHINOCA_IOS

Resource* createImage(const char* uri, ResourceManager* mgr);
bool loadImage(Resource* resource, ResourceManager* mgr);

Resource* createNSAudio(const char* uri, ResourceManager* mgr);
bool loadNSAudio(Resource* resource, ResourceManager* mgr);

#else

Resource* createBmp(const char* uri, ResourceManager* mgr);
bool loadBmp(Resource* resource, ResourceManager* mgr);

Resource* createPng(const char* uri, ResourceManager* mgr);
bool loadPng(Resource* resource, ResourceManager* mgr);

Resource* createJpeg(const char* uri, ResourceManager* mgr);
bool loadJpeg(Resource* resource, ResourceManager* mgr);

Resource* createMp3(const char* uri, ResourceManager* mgr);
bool loadMp3(Resource* resource, ResourceManager* mgr);

#endif

Resource* createText(const char* uri, ResourceManager* mgr);
bool loadText(Resource* resource, ResourceManager* mgr);

Resource* createTrueTypeFont(const char* uri, ResourceManager* mgr);
bool loadTrueTypeFont(Resource* resource, ResourceManager* mgr);

Resource* createOgg(const char* uri, ResourceManager* mgr);
bool loadOgg(Resource* resource, ResourceManager* mgr);

Resource* createWave(const char* uri, ResourceManager* mgr);
bool loadWave(Resource* resource, ResourceManager* mgr);

void registerLoaders(ResourceManager* mgr);

}	// namespace Loader

#endif	// __LOADER_LOADER_H__
