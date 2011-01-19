#ifndef __RENDER_RENDER_H__
#define __RENDER_RENDER_H__

namespace Render {

class Texture;

bool init();

void close();

class Driver
{
public:
	void setRenderTarget(Texture* texture);

	void setTransform(float* matrix);

	void drawTextureQuad(Texture* texture,
		unsigned srcX, unsigned srcY, unsigned srcH, unsigned srcW,
		unsigned dstX, unsigned dstY, unsigned dstH, unsigned dstW
	);
};

}	// Render

#endif	// __RENDER_RENDER_H__
