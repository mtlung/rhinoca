#ifndef __RENDER_DRIVER2_H__
#define __RENDER_DRIVER2_H__

#include <stddef.h>
#include "../rhinoca.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RhRenderDriverContext
{
	unsigned width, height;
	unsigned magjorVersion, minorVersion;
} RhRenderDriverContext;

typedef struct RhRenderTarget
{
	unsigned width, height;
} RhRenderTarget;

typedef enum RhRenderBufferType
{
	RhRenderBufferType_Vertex	= 1 << 0,	/// Vertex buffer
	RhRenderBufferType_Index	= 1 << 1,	/// Index buffer
	RhRenderBufferType_Uniform	= 1 << 2,	/// Shader uniform buffer
	RhRenderBufferType_System	= 1 << 3,	/// Indicating the buffer will hold in system memory
} RhRenderBufferType;

typedef enum RhRenderBufferMapUsage
{
	RhRenderBufferMapUsage_Read	= 1 << 0,
	RhRenderBufferMapUsage_Write= 1 << 1,
} RhRenderBufferMapUsage;

typedef struct RhRenderBuffer
{
	RhRenderBufferType type;
	unsigned sizeInBytes;
} RhRenderBuffer;

typedef enum RhRenderTextureFormat
{
	RhRenderTextureFormat_RGBA = 1,
	RhRenderTextureFormat_R,
	RhRenderTextureFormat_A,
	RhRenderTextureFormat_Depth,
	RhRenderTextureFormat_DepthStencil,

	RhRenderTextureFormat_Compressed= 0x00010000,
	RhRenderTextureFormat_PVRTC2	= 0x00010001,	// PowerVR texture compression 2-bpp
	RhRenderTextureFormat_PVRTC4	= 0x00010002,	// PowerVR texture compression 4-bpp
	RhRenderTextureFormat_DXT1		= 0x00010003,	// DirectX's DXT1 compression
	RhRenderTextureFormat_DXT5		= 0x00010004,	// DirectX's DXT5 compression
} RhRenderTextureFormat;

typedef struct RhRenderTexture
{
	unsigned width;
	unsigned height;
	RhRenderTextureFormat format;
} RhRenderTexture;

typedef enum RhRenderShaderType
{
	RhRenderShaderType_Vertex = 0,
	RhRenderShaderType_Pixel,
} RhRenderShaderType;

typedef struct RhRenderShader
{
	RhRenderShaderType type;
} RhRenderShader;

typedef struct RhRenderShaderProgram
{
} RhRenderShaderProgram;

typedef struct RhRenderShaderProgramInput
{
	RhRenderBuffer* buffer;
	const char* name;
	unsigned elementCount;	/// Can be 1, 2, 3, or 4
	unsigned offset;
	unsigned stride;
	unsigned nameHash;		/// after name is changed, reset it to zero will cause it to be re-calculated
	unsigned cacheId;
} RhRenderShaderProgramInput;

// 
typedef struct RhRenderDriver
{
// Context management
	RhRenderDriverContext* (*newContext)();
	void (*deleteContext)(RhRenderDriverContext* self);
	bool (*initContext)(RhRenderDriverContext* self, void* platformSpecificWindow);
	void (*useContext)(RhRenderDriverContext* self);

	void (*swapBuffers)();
	bool (*changeResolution)(unsigned width, unsigned height);
	void (*setViewport)(unsigned x, unsigned y, unsigned width, unsigned height);
	void (*clearColor)(float r, float g, float b, float a);
	void (*clearDepth)(float z);

// Render target
/*	RhRenderTarget* (*newRenderTarget)(
		void** textureHandle,
		void** depthHandle, void** stencilHandle,
		unsigned width, unsigned height
	);

	void (*deleteRenderTarget)(RhRenderTarget* self);*/

// Buffer
	RhRenderBuffer* (*newBuffer)();
	void (*deleteBuffer)(RhRenderBuffer* self);
	bool (*initBuffer)(RhRenderBuffer* self, RhRenderBufferType type, void* initData, unsigned sizeInBytes);
	bool (*updateBuffer)(RhRenderBuffer* self, unsigned offsetInBytes, void* data, unsigned sizeInBytes);
	void* (*mapBuffer)(RhRenderBuffer* self, RhRenderBufferMapUsage usage);
	void (*unmapBuffer)(RhRenderBuffer* self);

// Texture
	RhRenderTexture* (*newTexture)();
	void (*deleteTexture)(RhRenderTexture* self);
	bool (*initTexture)(RhRenderTexture* self, unsigned width, unsigned height, RhRenderTextureFormat format);	// Can be invoked in loader thread
	void (*commitTexture)(RhRenderTexture* self, void* data);	// Can only be invoked in render thread

// Shader
	RhRenderShader* (*newShader)();
	void (*deleteShader)(RhRenderShader* self);
	bool (*initShader)(RhRenderShader* self, RhRenderShaderType type, const char** sources, unsigned sourceCount);

	RhRenderShaderProgram* (*newShaderPprogram)();
	void (*deleteShaderProgram)(RhRenderShaderProgram* self);
	bool (*initShaderProgram)(RhRenderShaderProgram* self, RhRenderShader** shaders, unsigned shaderCount);

	bool (*setUniform1fv)(RhRenderShaderProgram* self, unsigned nameHash, const float* value, unsigned count);
	bool (*setUniform2fv)(RhRenderShaderProgram* self, unsigned nameHash, const float* value, unsigned count);
	bool (*setUniform3fv)(RhRenderShaderProgram* self, unsigned nameHash, const float* value, unsigned count);
	bool (*setUniform4fv)(RhRenderShaderProgram* self, unsigned nameHash, const float* value, unsigned count);
	bool (*setUniformMatrix4fv)(RhRenderShaderProgram* self, unsigned nameHash, bool transpose, const float* value, unsigned count);
	bool (*setUniformTexture)(RhRenderShaderProgram* self, unsigned nameHash, RhRenderTexture* texture);

	bool (*bindProgramInput)(RhRenderShaderProgram* self, RhRenderShaderProgramInput* inputs, unsigned inputCount, unsigned* cacheId);	// Give NULL to cacheId if you don't want to create a cache for it
	bool (*bindProgramInputCached)(RhRenderShaderProgram* self, unsigned cacheId);

// Making draw call
	void (*drawTriangle)(unsigned offset, unsigned vertexCount, unsigned flags);
	void (*drawTriangleIndexed)(unsigned offset, unsigned indexCount, unsigned flags);

// Driver version
	const char* driverName;
	unsigned majorVersion;
	unsigned minorVersion;
} RhRenderDriver;

RhRenderDriver* rhNewRenderDriver(const char* options);

void rhDeleteRenderDriver(RhRenderDriver* self);

#ifdef __cplusplus
}
#endif

#endif	// __RENDER_DRIVER2_H__
