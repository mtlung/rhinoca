#ifndef __RENDER_RENDERQUEUE_H__
#define __RENDER_RENDERQUEUE_H__

#include "../rhinoca.h"

namespace Render {

class RenderCommand
{
	virtual ~RenderCommand() {}
};

// A render queue in the spirit of http://realtimecollisiondetection.net/blog/?p=86
class RenderItemKey
{
public:
	int renderTarget;
	int	layer;
	int	viewport;
	int	viewportLayer;
	int	alphaBlend; 
	int	isCmd;

	int cmdPriority;
	int cmdParam;
	
	int mtlId;
	int mtlPass;
	float depth;	// 0 - 1, 24 bits resolution

	operator rhuint64();
	void decode(rhuint64 Key);

	void makeOutOfRange();
};

class RenderItem
{
public:
	rhuint64 key;
	RenderCommand* command;
};

class RenderQueue
{
public:
};

}	// Render

#endif	// __RENDER_RENDERQUEUE_H__
