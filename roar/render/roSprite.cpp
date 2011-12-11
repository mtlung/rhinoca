#include "pch.h"

#include "roSprite.h"

#include "roRenderDriver.h"

#include <cassert>
#include <math.h>

namespace ro 
{

roSpriteFrame::roSpriteFrame()
:	width(-1)
,	height(-1)
,	u0(0.0f)
,	v0(0.0f)
,	u1(1.0f)
,	v1(1.0f)
{	
}

roSprite::roSprite()
:	accuTime(0.0f)
,	totalTime(0.0f)
,	isPlaying(false)
,	currentFrameIndex(-1)
,	isDirty(false)
{
}

void roSprite::Tick(float dt)
{
	accuTime += dt;
	float f = fmod(accuTime,totalTime);
	currentFrameIndex = 0;
	for(int j=0;j<(int)frames.size();++j)
	{
		f -= frames[j].duration;
		if( f <= 0.0f )
		{
			currentFrameIndex = j;
			break;
		}
	}
}

int roSprite::GetCurrentFrameIndex() const
{
	return currentFrameIndex;
}

void roSprite::Play()
{
	isPlaying = true;
	accuTime = 0.0f;

	if( isDirty )
	{
		Update();
	}
	
	currentFrameIndex = frames.size() > 0 ? 0 : -1;
}

void roSprite::GoToAndPlay(int frame)
{
	Update();

	assert( 0<=frame && frame < (int)frames.size() );
	isPlaying = true;
	accuTime = 0.0f;
	for(int j=0;j<frame;++j)
	{
		accuTime += frames[j].duration;
	}
}

void roSprite::AddFrame(const roSpriteFrame & frame)
{
	frames.pushBack(frame);	
	isDirty = true;
}

void roSprite::Update()
{
	if( isDirty )
	{
		totalTime = 0.0f;
		for(size_t j=0;j<frames.size();++j)
		{
			totalTime += frames[j].duration;	
		}
	}
}

}