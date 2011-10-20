#ifndef __RENDER_DRIVER2_H__
#define __RENDER_DRIVER2_H__

#include <stddef.h>
#include "../rhinoca.h"

#ifdef __cplusplus
extern "C" {
#endif

struct RgDriver;

typedef struct RgDriverContext
{
	RgDriver* driver;
	unsigned width, height;
	unsigned magjorVersion, minorVersion;
} RgDriverContext;

typedef struct RgDriverTarget
{
	unsigned width, height;
} RgDriverTarget;

typedef enum RgDriverBufferType
{
	RgDriverBufferType_Vertex	= 1,	/// Vertex buffer
	RgDriverBufferType_Index	= 2,	/// Index buffer
	RgDriverBufferType_Uniform	= 3,	/// Shader uniform buffer
	RgDriverBufferType_System	= 1 << 3,	/// Indicating the buffer will hold in system memory
} RgDriverBufferType;

typedef enum RgDriverBufferMapUsage
{
	RgDriverBufferMapUsage_Read	= 1 << 0,
	RgDriverBufferMapUsage_Write= 1 << 1,
} RgDriverBufferMapUsage;

typedef struct RgDriverBuffer
{
	RgDriverBufferType type;
	unsigned sizeInBytes;
} RgDriverBuffer;

typedef enum RgDriverTextureFormat
{
	RgDriverTextureFormat_RGBA = 1,
	RgDriverTextureFormat_R,
	RgDriverTextureFormat_A,
	RgDriverTextureFormat_Depth,
	RgDriverTextureFormat_DepthStencil,

	RgDriverTextureFormat_Compressed= 0x00010000,
	RgDriverTextureFormat_PVRTC2	= 0x00010001,	/// PowerVR texture compression 2-bpp
	RgDriverTextureFormat_PVRTC4	= 0x00010002,	/// PowerVR texture compression 4-bpp
	RgDriverTextureFormat_DXT1		= 0x00010003,	/// DirectX's DXT1 compression
	RgDriverTextureFormat_DXT5		= 0x00010004,	/// DirectX's DXT5 compression
} RgDriverTextureFormat;

typedef struct RgDriverTexture
{
	unsigned width;
	unsigned height;
	RgDriverTextureFormat format;
} RgDriverTexture;

typedef enum RgDriverShaderType
{
	RgDriverShaderType_Vertex = 0,
	RgDriverShaderType_Pixel,
	RgDriverShaderType_Geometry,
} RgDriverShaderType;

typedef struct RgDriverShader
{
	RgDriverShaderType type;
} RgDriverShader;

typedef struct RgDriverShaderInput
{
	RgDriverBuffer* buffer;
	RgDriverShader* shader;
	const char* name;
	unsigned elementCount;	/// Can be 1, 2, 3, or 4
	unsigned offset;
	unsigned stride;
	unsigned nameHash;		/// after name is changed, reset it to zero will cause it to be re-calculated
	unsigned cacheId;
} RgDriverShaderInput;

typedef enum RgDriverBlendOp
{
	RgDriverBlendOp_Add = 0,
	RgDriverBlendOp_Subtract,
	RgDriverBlendOp_RevSubtract,
	RgDriverBlendOp_Min,
	RgDriverBlendOp_Max,
} RgDriverBlendOp;

typedef enum RgDriverBlendValue
{
	RgDriverBlendValue_Zero = 0,
	RgDriverBlendValue_One,
	RgDriverBlendValue_SrcColor,
	RgDriverBlendValue_InvSrcColor,
	RgDriverBlendValue_SrcAlpha,
	RgDriverBlendValue_InvSrcAlpha,
	RgDriverBlendValue_DstColor,
	RgDriverBlendValue_InvDstColor,
	RgDriverBlendValue_DstAlpha,
	RgDriverBlendValue_InvDstAlpha,
} RgDriverBlendValue;

typedef enum RgDriverColorWriteMask
{
	RgDriverColorWriteMask_DisableAll	= 0,
	RgDriverColorWriteMask_EnableRed	= 1,
	RgDriverColorWriteMask_EnableGreen	= 2,
	RgDriverColorWriteMask_EnableBlue	= 4,
	RgDriverColorWriteMask_EnableAlpha	= 8,
	RgDriverColorWriteMask_EnableAll	= 1 | 2 | 4 | 8,
} RgDriverColorWriteMask;

typedef struct RgDriverBlendState
{
	void* hash;		/// Set it to 0 when ever the state is changed
	unsigned enable;	/// Use unsigned to avoid any padding
	RgDriverBlendOp colorOp, alphaOp;
	RgDriverBlendValue colorSrc, colorDst, alphaSrc, alphaDst;
	RgDriverColorWriteMask wirteMask;
} RgDriverBlendState;

typedef enum RgDriverCompareFunc
{
	RgDriverDepthCompareFunc_Never = 0,	/// Never pass, 
	RgDriverDepthCompareFunc_Less,		/// Pass if src depth < dst depth, pass if (ref & mask) < (stencil & mask)
	RgDriverDepthCompareFunc_Equal,
	RgDriverDepthCompareFunc_LEqual,
	RgDriverDepthCompareFunc_Greater,
	RgDriverDepthCompareFunc_NotEqual,
	RgDriverDepthCompareFunc_GEqual,
	RgDriverDepthCompareFunc_Always,
} RgDriverDepthCompareFunc;

typedef enum RgDriverStencilOp
{
	RgDriverStencilOp_Zero = 0,
	RgDriverStencilOp_Invert,
	RgDriverStencilOp_Keep,
	RgDriverStencilOp_Replace,
	RgDriverStencilOp_Incr,
	RgDriverStencilOp_Decr,
	RgDriverStencilOp_IncrWrap,
	RgDriverStencilOp_DecrWrap,
};

typedef struct RgDriverStencilState
{
	RgDriverCompareFunc func;
	RgDriverStencilOp failOp, zFailOp, passOp;
} RgDriverStencilState;

typedef struct RgDriverDepthStencilState
{
	void* hash;		/// Set it to 0 when ever the state is changed
	unsigned short enableDepth;
	unsigned short enableStencil;
	RgDriverCompareFunc depthFunc;

	unsigned short stencilRefValue;
	unsigned short stencilMask;
	RgDriverStencilState front, back;
} RgDriverDepthStencilState;

typedef enum RgDriverTextureFilterMode
{
	RgDriverTextureFilterMode_MinMagPoint = 0,
	RgDriverTextureFilterMode_MinMagLinear,
	RgDriverTextureFilterMode_MipMagPoint,
	RgDriverTextureFilterMode_MipMagLinear
} RgDriverTextureFilterMode;

typedef enum RgDriverTextureAddressMode
{
	RgDriverTextureAddressMode_Repeat = 0,
	RgDriverTextureAddressMode_Edge,
	RgDriverTextureAddressMode_Border,
	RgDriverTextureAddressMode_MirrorRepeat
} RgDriverTextureAddressMode;

typedef struct RgDriverTextureState
{
	void* hash;		/// Set it to 0 when ever the state is changed
	RgDriverTextureFilterMode filter;
	RgDriverTextureAddressMode u, v;
	unsigned maxAnisotropy;
} RgDriverTextureState;	// RgDriverTextureState

// Default states:
// CCW -> front face
typedef struct RgDriver
{
// Context management
	RgDriverContext* (*newContext)(RgDriver* driver);
	void (*deleteContext)(RgDriverContext* self);
	bool (*initContext)(RgDriverContext* self, void* platformSpecificWindow);
	void (*useContext)(RgDriverContext* self);
	RgDriverContext* (*currentContext)();

	void (*swapBuffers)();
	bool (*changeResolution)(unsigned width, unsigned height);
	void (*setViewport)(unsigned x, unsigned y, unsigned width, unsigned height, float zmin, float zmax);
	void (*clearColor)(float r, float g, float b, float a);
	void (*clearDepth)(float z);
	void (*clearStencil)(unsigned char s);

// State management
	void (*applyDefaultState)(RgDriverContext* self);
	void (*setBlendState)(RgDriverBlendState* blendState);
	void (*setDepthStencilState)(RgDriverDepthStencilState* depthStencilState);
	void (*setTextureState)(RgDriverTextureState* textureStates, unsigned stateCount, unsigned startingTextureUnit);

// Render target
/*	RgDriverTarget* (*newRenderTarget)(
		void** textureHandle,
		void** depthHandle, void** stencilHandle,
		unsigned width, unsigned height
	);

	void (*deleteRenderTarget)(RgDriverTarget* self);*/

// Buffer
	RgDriverBuffer* (*newBuffer)();
	void (*deleteBuffer)(RgDriverBuffer* self);
	bool (*initBuffer)(RgDriverBuffer* self, RgDriverBufferType type, void* initData, unsigned sizeInBytes);
	bool (*updateBuffer)(RgDriverBuffer* self, unsigned offsetInBytes, void* data, unsigned sizeInBytes);
	void* (*mapBuffer)(RgDriverBuffer* self, RgDriverBufferMapUsage usage);
	void (*unmapBuffer)(RgDriverBuffer* self);

// Texture
	RgDriverTexture* (*newTexture)();
	void (*deleteTexture)(RgDriverTexture* self);
	bool (*initTexture)(RgDriverTexture* self, unsigned width, unsigned height, RgDriverTextureFormat format);	// Can be invoked in loader thread
	bool (*commitTexture)(RgDriverTexture* self, const void* data, unsigned rowPaddingInBytes);	// Can only be invoked in render thread

// Shader
	RgDriverShader* (*newShader)();
	void (*deleteShader)(RgDriverShader* self);
	bool (*initShader)(RgDriverShader* self, RgDriverShaderType type, const char** sources, unsigned sourceCount);

	bool (*bindShaders)(RgDriverShader** shaders, unsigned shaderCount);
	bool (*setUniform1fv)(unsigned nameHash, const float* value, unsigned count);
	bool (*setUniform2fv)(unsigned nameHash, const float* value, unsigned count);
	bool (*setUniform3fv)(unsigned nameHash, const float* value, unsigned count);
	bool (*setUniform4fv)(unsigned nameHash, const float* value, unsigned count);
	bool (*setUniformMat44fv)(unsigned nameHash, bool transpose, const float* value, unsigned count);
	bool (*setUniformTexture)(unsigned nameHash, RgDriverTexture* texture);

	bool (*bindShaderInput)(RgDriverShaderInput* inputs, unsigned inputCount, unsigned* cacheId);	// Give NULL to cacheId if you don't want to create a cache for it
	bool (*bindShaderInputCached)(unsigned cacheId);

// Making draw call
	void (*drawTriangle)(unsigned offset, unsigned vertexCount, unsigned flags);
	void (*drawTriangleIndexed)(unsigned offset, unsigned indexCount, unsigned flags);

// Driver version
	const char* driverName;
	unsigned majorVersion;
	unsigned minorVersion;
	void (*destructor)(RgDriver* self);
} RgDriver;

RgDriver* rgNewRenderDriver(const char* driverType, const char* options);

void rgDeleteRenderDriver(RgDriver* self);

#ifdef __cplusplus
}
#endif

#endif	// __RENDER_DRIVER2_H__
