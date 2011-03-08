#ifndef __RENDER_DRIVERDETAIL_H__
#define __RENDER_DRIVERDETAIL_H__

namespace Render {

struct Mesh
{
	Driver::MeshFormat format;

	// Either buffer or handle will be not null
	char* vb, *ib;	// Pointer to buffer data in CPU memory
	void* vh, *ih;	// Handle to API sepecific handle (data in GPU)

	unsigned vertexCount, indexCount;
};

struct MeshBuilder
{
	MeshBuilder();
	~MeshBuilder();

	Driver::MeshFormat format;

	// Current values
	float normal[3];
	rhuint8 color[4];
	unsigned texUnit;
	float uv[Driver::maxTextureUnit][2];

	unsigned vertexCount;
	unsigned indexCount;

	unsigned vertexCapacity;	// How many vertex does the current buffer can store
	unsigned indexCapacity;		// How many index does the current buffer can store

	char* vertexBuffer;
	rhuint16* indexBuffer;
};

MeshBuilder* currentMeshBuilder();

}	// Render

#endif	// __RENDER_DRIVERDETAIL_H__
