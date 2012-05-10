#include "pch.h"
#include "loader.h"
#include "../roar/base/roResource.h"

namespace Loader {

void registerLoaders(ro::ResourceManager* mgr)
{/*
#ifdef RHINOCA_IOS
	mgr->addFactory(createImage, loadImage);
	mgr->addFactory(createNSAudio, loadNSAudio);
#else
	mgr->addFactory(createBmp, loadBmp);
	mgr->addFactory(createPng, loadPng);
	mgr->addFactory(createJpeg, loadJpeg);
	mgr->addFactory(createMp3, loadMp3);
#endif
	mgr->addFactory(createText, loadText);
	mgr->addFactory(createTrueTypeFont, loadTrueTypeFont);
	mgr->addFactory(createOgg, loadOgg);
	mgr->addFactory(createWave, loadWave);*/
}

}	// namespace Loader
