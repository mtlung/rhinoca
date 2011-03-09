#include "pch.h"
#include "driver.h"
#include "driverdetail.h"
#include "../mat44.h"
#include "../common.h"

namespace Render {

unsigned Driver::nextPowerOfTwo(unsigned x)
{
	x = x - 1;
	x = x | (x >> 1);
	x = x | (x >> 2);
	x = x | (x >> 4);
	x = x | (x >> 8);
	x = x | (x >>16);
	return x + 1;
}

void Driver::ortho(float left, float right, float bottom, float top, float zNear, float zFar)
{
	Mat44 m = Mat44::identity;

	m.m00 = 2.0f / (right - left);
	m.m03 = -(right + left) / (right - left);
	m.m11 = 2.0f / (top - bottom);
	m.m13 = -(top + bottom) / (top - bottom);
	m.m22 = -2.0f / (zFar - zNear);
	m.m23 = -(zFar + zNear) / (zFar - zNear);
	m.m33 = 1;

	Driver::setProjectionMatrix(m.data);
}

void Driver::ortho(unsigned left, unsigned right, unsigned bottom, unsigned top, unsigned zNear, unsigned zFar)
{
	ortho((float)left, (float)right, (float)bottom, (float)top, (float)zNear, (float)zFar);
}

void Driver::drawQuad(
	float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, float z,
	float r, float g, float b, float a
)
{
	Driver::drawQuad(
		x1, y1, x2, y2,
		x3, y3, x4, y4,
		z,
		(rhuint8)(r * 255), (rhuint8)(g * 255), (rhuint8)(b * 255), (rhuint8)(a * 255)
	);
}

BufferBuilder::BufferBuilder()
	: texUnit(0)
	, vertexBuffer(NULL), vertexCount(0), vertexCapacity(0)
	, indexBuffer(NULL), indexCount(0), indexCapacity(0)
{
}

BufferBuilder::~BufferBuilder()
{
	rhinoca_free(vertexBuffer);
	rhinoca_free(indexBuffer);
}

unsigned vertexSizeForFormat(Driver::VertexFormat format)
{
	static const unsigned sf = sizeof(float);
	static const unsigned sb = 1;

	switch(format) {
	case Driver::P_UV0: return 3*sf + 2*sf;	// 20 bytes
	case Driver::P_C: return 3*sf + 4*sb;	// 16 bytes
	case Driver::P_C_UV0: return 3*sf + 4*sb + 2*sf;	// 24 bytes
	case Driver::P_N_UV0_UV1: return 3*sf + 3*sf + 2*sf + 2*sf;	// 40 bytes
	default: return 0;
	}
}

void Driver::beginVertex(VertexFormat format)
{
	BufferBuilder* builder = currentBufferBuilder();
	builder->format = format;
	builder->vertexCount = 0;
}

rhuint16 Driver::vertex3f(float x, float y, float z)
{
	BufferBuilder* builder = currentBufferBuilder();

	ASSERT("Too many vertice" && builder->vertexCount < rhuint16(-1));

	const unsigned vertexSize = vertexSizeForFormat(builder->format);

	// Prepare vertex buffer memory
	if(builder->vertexCapacity <= builder->vertexCount) {
		if(builder->vertexCapacity == 0)
			builder->vertexCapacity = 64;
		else
			builder->vertexCapacity *= 2;

		builder->vertexBuffer = (char*)rhinoca_realloc(
			builder->vertexBuffer,
			builder->vertexCount * vertexSize,
			builder->vertexCapacity * vertexSize
		);
	}

	{	// Assigning position
		float* p = (float*)(builder->vertexBuffer + builder->vertexCount * vertexSize);
		p[0] = x;
		p[1] = y;
		p[2] = z;
	}

	{	// Assigning normal
		float* n = NULL;
		switch(builder->format) {
		case P_N_UV0_UV1:
			n = (float*)(builder->vertexBuffer + builder->vertexCount * vertexSize + 3 * sizeof(float));
			break;
		}

		if(n) {
			n[0] = builder->normal[0];
			n[1] = builder->normal[1];
			n[2] = builder->normal[2];
		}
	}

	{	// Assigning color
		rhuint8* c = NULL;
		switch(builder->format) {
		case P_C:
		case P_C_UV0:
			c = (rhuint8*)(builder->vertexBuffer + builder->vertexCount * vertexSize + 3 * sizeof(float));
			break;
		}

		if(c) {
			c[0] = builder->color[0];
			c[1] = builder->color[1];
			c[2] = builder->color[2];
			c[3] = builder->color[3];
		}
	}

	return builder->vertexCount++;
}

void Driver::normal3f(float x, float y, float z)
{
	BufferBuilder* builder = currentBufferBuilder();
	builder->normal[0] = x;
	builder->normal[1] = y;
	builder->normal[2] = z;
}

void Driver::texUnit(unsigned unit)
{
	BufferBuilder* builder = currentBufferBuilder();
	builder->texUnit = unit;
}

void Driver::texCoord2f(float u, float v)
{
	BufferBuilder* builder = currentBufferBuilder();
	float* uv = builder->uv[builder->texUnit];
	uv[0] = u;
	uv[1] = v;
}

void* Driver::endVertex()
{
	BufferBuilder* builder = currentBufferBuilder();

	return Driver::createVertexCopyData(builder->format, builder->vertexBuffer, builder->vertexCount);
}

void Driver::beginIndex()
{
	BufferBuilder* builder = currentBufferBuilder();
	builder->indexCount = 0;
}

void Driver::addIndex(rhuint16 idx)
{
	addIndex(&idx, 1);
}

void Driver::addIndex(rhuint16* idx, unsigned indexCount)
{
	BufferBuilder* builder = currentBufferBuilder();

	// Prepare index buffer memory
	if(builder->indexCapacity < builder->indexCount + indexCount) {
		if(builder->indexCapacity == 0)
			builder->indexCapacity = 64 + indexCount;
		else
			builder->indexCapacity = (builder->indexCount + indexCount) * 2;

		builder->indexBuffer = (rhuint16*)rhinoca_realloc(
			builder->indexBuffer,
			builder->indexCount * sizeof(rhuint16),
			builder->indexCapacity * sizeof(rhuint16)
		);
	}

	rhuint16* ib = builder->indexBuffer + builder->indexCount;

	for(unsigned i=0; i<indexCount; ++i)
		ib[i] = idx[i];

	builder->indexCount += indexCount;
}

void* Driver::endIndex()
{
	BufferBuilder* builder = currentBufferBuilder();

	return Driver::createIndexCopyData(builder->indexBuffer, builder->indexCount);
}

}	// Render
