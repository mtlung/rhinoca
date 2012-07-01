#include "pch.h"
#include "../../roar/audio/roAudioDriver.h"
#include "../../roar/base/roTaskPool.h"
#include "../../roar/roSubSystems.h"
#include <conio.h>

using namespace ro;

struct AudioTest
{
	ro::SubSystems subSystems;
};

TEST_FIXTURE(AudioTest, soundSource)
{
	subSystems.init();
	roAudioDriver* driver = subSystems.audioDriver;

	roADriverSoundSource* sound = driver->newSoundSource(driver, "aron.mp3", "mp3", true);
	driver->playSoundSource(sound);
	driver->soundSourceSetLoop(sound, true);

	bool play = true;

	while(true) {
		int cmd;
		if(kbhit()) {
			cmd = getch();

			if(cmd == 'q')
				break;

			if(cmd == 'p') {
				play = !play;
				if(play)	driver->playSoundSource(sound);
				else		driver->stopSoundSource(sound);
			}

			if(cmd == 'r')
				driver->soundSourceSeekPos(sound, 0);

			if(cmd == '>')
				driver->soundSourceSeekPos(sound, driver->soundSourceTellPos(sound) + 10);
			if(cmd == '<')
				driver->soundSourceSeekPos(sound, driver->soundSourceTellPos(sound) - 10);
		}

		TaskPool::sleep(10);

		printf("%f\n", driver->soundSourceTellPos(sound));
		subSystems.tick();
	}

	driver->deleteSoundSource(sound);
}
