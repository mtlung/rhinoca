#ifndef __LOADER_LOADER_H__
#define __LOADER_LOADER_H__

class Resource;
class ResourceManager;

namespace Loader {

Resource* createBmp(const char* uri, ResourceManager* mgr);
bool loadBmp(Resource* resource, ResourceManager* mgr);

Resource* createPng(const char* uri, ResourceManager* mgr);
bool loadPng(Resource* resource, ResourceManager* mgr);

Resource* createJpeg(const char* uri, ResourceManager* mgr);
bool loadJpeg(Resource* resource, ResourceManager* mgr);

}	// namespace Loader

#endif	// __LOADER_LOADER_H__
