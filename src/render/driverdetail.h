#ifndef __RENDER_DRIVERDETAIL_H__
#define __RENDER_DRIVERDETAIL_H__

namespace Render {

struct VertexBuffer
{
	// Either data or handle will be not null
	void* data;
	void* handle;
	unsigned count;
	Driver::VertexFormat format;
};

struct IndexBuffer
{
	// Either data or handle will be not null
	void* data;
	void* handle;
	unsigned count;
};

unsigned vertexSizeForFormat(Driver::VertexFormat format);

struct BufferBuilder
{
	BufferBuilder();
	~BufferBuilder();

	Driver::VertexFormat format;

	// Current values
	float normal[3];
	rhuint8 color[4];
	unsigned texUnit;
	float uv[Driver::maxTextureUnit][2];

	char* vertexBuffer;
	unsigned vertexCount;
	unsigned vertexCapacity;	// How many vertex does the current buffer can store

	rhuint16* indexBuffer;
	unsigned indexCount;
	unsigned indexCapacity;		// How many index does the current buffer can store
};

BufferBuilder* currentBufferBuilder();

}	// Render

#endif	// __RENDER_DRIVERDETAIL_H__
