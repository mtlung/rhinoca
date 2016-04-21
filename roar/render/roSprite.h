#ifndef __render_roSprite_h__
#define __render_roSprite_h__

#include "../base/roArray.h"

namespace ro 
{

class roSpriteFrame
{
public:
	roSpriteFrame();
	int width;
	int height;
	float u0;
	float v0;
	float u1;
	float v1;
	float duration;
};

class roSprite
{
	enum roPlayType
	{
		Loop = 0,
		PlayOneAndStop,
		PingPong
	};

public:
	roSprite();
	void Tick(float dt);

	void AddFrame(const roSpriteFrame & frame);

	void Play();
	void GoToAndPlay(int frame);
	void Update();

	int GetCurrentFrameIndex() const;
	const roSpriteFrame & GetCurrentFrame() const;
	
protected:
	int currentFrameIndex;
	bool isPlaying;
	float accuTime;
	float totalTime;	
	bool isDirty;
	Array<roSpriteFrame> frames;
};	// roSprite

}
#endif