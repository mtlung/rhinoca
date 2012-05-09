#ifndef __LOADER_LOADER_H__
#define __LOADER_LOADER_H__

#include "rhinoca.h"

namespace ro {

struct Resource;
struct ResourceManager;

}	// namespace ro

namespace Loader {

#ifdef RHINOCA_IOS

Resource* createImage(const char* uri, ResourceManager* mgr);
bool loadImage(Resource* resource, ResourceManager* mgr);

Resource* createNSAudio(const char* uri, ResourceManager* mgr);
bool loadNSAudio(Resource* resource, ResourceManager* mgr);

#else

ro::Resource* createBmp(const char* uri, ro::ResourceManager* mgr);
bool loadBmp(ro::Resource* resource, ro::ResourceManager* mgr);

ro::Resource* createPng(const char* uri, ro::ResourceManager* mgr);
bool loadPng(ro::Resource* resource, ro::ResourceManager* mgr);

ro::Resource* createJpeg(const char* uri, ro::ResourceManager* mgr);
bool loadJpeg(ro::Resource* resource, ro::ResourceManager* mgr);

ro::Resource* createMp3(const char* uri, ro::ResourceManager* mgr);
bool loadMp3(ro::Resource* resource, ro::ResourceManager* mgr);

#endif

ro::Resource* createTrueTypeFont(const char* uri, ro::ResourceManager* mgr);
bool loadTrueTypeFont(ro::Resource* resource, ro::ResourceManager* mgr);

ro::Resource* createOgg(const char* uri, ro::ResourceManager* mgr);
bool loadOgg(ro::Resource* resource, ro::ResourceManager* mgr);

ro::Resource* createWave(const char* uri, ro::ResourceManager* mgr);
bool loadWave(ro::Resource* resource, ro::ResourceManager* mgr);

void registerLoaders(ro::ResourceManager* mgr);

}	// namespace Loader

#endif	// __LOADER_LOADER_H__
