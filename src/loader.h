#ifndef __LOADER_LOADER_H__
#define __LOADER_LOADER_H__

#include "rhinoca.h"

class Resource;
class ResourceManager;

namespace Loader {

#ifdef RHINOCA_IOS

Resource* createImage(const char* uri, ResourceManager* mgr);
bool loadImage(Resource* resource, ResourceManager* mgr);

#else

Resource* createBmp(const char* uri, ResourceManager* mgr);
bool loadBmp(Resource* resource, ResourceManager* mgr);

Resource* createPng(const char* uri, ResourceManager* mgr);
bool loadPng(Resource* resource, ResourceManager* mgr);

Resource* createJpeg(const char* uri, ResourceManager* mgr);
bool loadJpeg(Resource* resource, ResourceManager* mgr);

#endif

Resource* createOgg(const char* uri, ResourceManager* mgr);
bool loadOgg(Resource* resource, ResourceManager* mgr);

Resource* createWave(const char* uri, ResourceManager* mgr);
bool loadWave(Resource* resource, ResourceManager* mgr);

void registerLoaders(ResourceManager* mgr);

}	// namespace Loader

#endif	// __LOADER_LOADER_H__
