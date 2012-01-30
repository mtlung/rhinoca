#ifndef __render_roRenderDriver_h__
#define __render_roRenderDriver_h__

#include "../platform/roCompiler.h"

#ifdef __cplusplus
extern "C" {
#endif

struct roRDriver;

typedef struct roRDriverContext
{
	roRDriver* driver;
	unsigned width, height;
	unsigned magjorVersion, minorVersion;
	unsigned frameCount;
	float lastFrameDuration;
	float lastSwapTime;
} roRDriverContext;

typedef struct roRDriverTarget
{
	unsigned width, height;
} roRDriverTarget;

typedef enum roRDriverBufferType
{
	roRDriverBufferType_Vertex	= 1,	/// Vertex buffer
	roRDriverBufferType_Index	= 2,	/// Index buffer
	roRDriverBufferType_Uniform	= 3,	/// Shader uniform buffer
} roRDriverBufferType;

typedef enum roRDriverDataUsage
{
	roRDriverDataUsage_Static	= 1,	/// No access for CPU after initialized
	roRDriverDataUsage_Stream	= 2,	/// Upload to the GPU for every draw call
	roRDriverDataUsage_Dynamic	= 3,	/// Write by CPU a few times and read by GPU for many times
} roRDriverDataUsage;

typedef enum roRDriverBufferMapUsage
{
	roRDriverBufferMapUsage_Read	= 1 << 0,
	roRDriverBufferMapUsage_Write	= 1 << 1,
} roRDriverBufferMapUsage;

typedef struct roRDriverBuffer
{
	roRDriverBufferType type : 8;
	roRDriverDataUsage usage : 8;
	unsigned isMapped : 8;
	roRDriverBufferMapUsage mapUsage : 8;
	roSize sizeInBytes;
} roRDriverBuffer;

typedef enum roRDriverTextureFormat
{
	roRDriverTextureFormat_RGBA = 1,
	roRDriverTextureFormat_R,
	roRDriverTextureFormat_A,
	roRDriverTextureFormat_Depth,
	roRDriverTextureFormat_DepthStencil,

	roRDriverTextureFormat_Compressed= 0x00010000,
	roRDriverTextureFormat_PVRTC2	= 0x00010001,	/// PowerVR texture compression 2-bpp
	roRDriverTextureFormat_PVRTC4	= 0x00010002,	/// PowerVR texture compression 4-bpp
	roRDriverTextureFormat_DXT1		= 0x00010003,	/// DirectX's DXT1 compression
	roRDriverTextureFormat_DXT5		= 0x00010004,	/// DirectX's DXT5 compression
} roRDriverTextureFormat;

typedef enum roRDriverTextureFlag
{
	roRDriverTextureFlag_None = 0,
	roRDriverTextureFlag_RenderTarget,
} roRDriverTextureFlag;

typedef struct roRDriverTexture
{
	unsigned width;
	unsigned height;
	roRDriverTextureFormat format;
	roRDriverTextureFlag flags;
} roRDriverTexture;

typedef enum roRDriverShaderType
{
	roRDriverShaderType_Vertex	= 0,
	roRDriverShaderType_Pixel	= 1,
	roRDriverShaderType_Geometry= 2,
} roRDriverShaderType;

typedef struct roRDriverShader
{
	roRDriverShaderType type;
} roRDriverShader;

typedef struct roRDriverShaderInput
{
	roRDriverBuffer* buffer;
	roRDriverShader* shader;
	const char* name;
	unsigned nameHash;		/// after name is changed, reset it to zero will cause it to be re-calculated
	unsigned offset;
	unsigned stride;
	unsigned cacheId;
} roRDriverShaderInput;

typedef enum roRDriverBlendOp
{
	roRDriverBlendOp_Add = 1,
	roRDriverBlendOp_Subtract,
	roRDriverBlendOp_RevSubtract,
	roRDriverBlendOp_Min,
	roRDriverBlendOp_Max,
} roRDriverBlendOp;

typedef enum roRDriverBlendValue
{
	roRDriverBlendValue_Zero = 1,
	roRDriverBlendValue_One,
	roRDriverBlendValue_SrcColor,
	roRDriverBlendValue_InvSrcColor,
	roRDriverBlendValue_SrcAlpha,
	roRDriverBlendValue_InvSrcAlpha,
	roRDriverBlendValue_DstColor,
	roRDriverBlendValue_InvDstColor,
	roRDriverBlendValue_DstAlpha,
	roRDriverBlendValue_InvDstAlpha,
} roRDriverBlendValue;

typedef enum roRDriverColorWriteMask
{
	roRDriverColorWriteMask_DisableAll	= 0,
	roRDriverColorWriteMask_EnableRed	= 1,
	roRDriverColorWriteMask_EnableGreen	= 2,
	roRDriverColorWriteMask_EnableBlue	= 4,
	roRDriverColorWriteMask_EnableAlpha	= 8,
	roRDriverColorWriteMask_EnableAll	= 1 | 2 | 4 | 8,
} roRDriverColorWriteMask;

typedef struct roRDriverBlendState
{
	void* hash;			/// Set it to 0 when ever the state is changed
	unsigned enable;	/// Use unsigned to avoid any padding
	roRDriverBlendOp colorOp, alphaOp;
	roRDriverBlendValue colorSrc, colorDst, alphaSrc, alphaDst;
	roRDriverColorWriteMask wirteMask;
} roRDriverBlendState;

typedef struct roRDriverRasterizerState
{
	void* hash;		/// Set it to 0 when ever the state is changed
	bool scissorEnable;
} roRDriverRasterizerState;

typedef enum roRDriverCompareFunc
{
	roRDriverDepthCompareFunc_Never = 1,/// Never pass, 
	roRDriverDepthCompareFunc_Less,		/// Pass if src depth < dst depth, pass if (ref & mask) < (stencil & mask)
	roRDriverDepthCompareFunc_Equal,
	roRDriverDepthCompareFunc_LEqual,
	roRDriverDepthCompareFunc_Greater,
	roRDriverDepthCompareFunc_NotEqual,
	roRDriverDepthCompareFunc_GEqual,
	roRDriverDepthCompareFunc_Always,
} roRDriverDepthCompareFunc;

typedef enum roRDriverStencilOp
{
	roRDriverStencilOp_Zero = 1,
	roRDriverStencilOp_Invert,
	roRDriverStencilOp_Keep,
	roRDriverStencilOp_Replace,
	roRDriverStencilOp_Incr,
	roRDriverStencilOp_Decr,
	roRDriverStencilOp_IncrWrap,
	roRDriverStencilOp_DecrWrap,
};

typedef struct roRDriverStencilState
{
	roRDriverCompareFunc func;
	roRDriverStencilOp failOp, zFailOp, passOp;
} roRDriverStencilState;

typedef struct roRDriverDepthStencilState
{
	void* hash;		/// Set it to 0 whenever the state is changed
	unsigned short enableDepth;
	unsigned short enableStencil;
	roRDriverCompareFunc depthFunc;

	unsigned short stencilRefValue;
	unsigned short stencilMask;
	roRDriverStencilState front, back;
} roRDriverDepthStencilState;

typedef enum roRDriverTextureFilterMode
{
	roRDriverTextureFilterMode_MinMagPoint = 1,
	roRDriverTextureFilterMode_MinMagLinear,
	roRDriverTextureFilterMode_MipMagPoint,
	roRDriverTextureFilterMode_MipMagLinear
} roRDriverTextureFilterMode;

typedef enum roRDriverTextureAddressMode
{
	roRDriverTextureAddressMode_Repeat = 1,
	roRDriverTextureAddressMode_Edge,
	roRDriverTextureAddressMode_Border,
	roRDriverTextureAddressMode_MirrorRepeat
} roRDriverTextureAddressMode;

typedef struct roRDriverTextureState
{
	void* hash;		/// Set it to 0 when ever the state is changed
	roRDriverTextureFilterMode filter;
	roRDriverTextureAddressMode u, v;
	unsigned maxAnisotropy;
} roRDriverTextureState;

typedef enum roRDriverPrimitiveType
{
	roRDriverPrimitiveType_PointList = 0,
	roRDriverPrimitiveType_Linelist,
	roRDriverPrimitiveType_LineStrip,
	roRDriverPrimitiveType_TriangleList,
	roRDriverPrimitiveType_TriangleStrip,
	roRDriverPrimitiveType_TriangleFan	// NOTE: Not supported on DX11
} roRDriverPrimitiveType;

// Default states:
// CCW -> front face
typedef struct roRDriver
{
// Context management
	roRDriverContext* (*newContext)(roRDriver* driver);
	void (*deleteContext)(roRDriverContext* self);
	bool (*initContext)(roRDriverContext* self, void* platformSpecificWindow);
	void (*useContext)(roRDriverContext* self);
	roRDriverContext* (*currentContext)();

	void (*swapBuffers)();
	bool (*changeResolution)(unsigned width, unsigned height);
	void (*setViewport)(unsigned x, unsigned y, unsigned width, unsigned height, float zmin, float zmax);
	void (*setScissorRect)(unsigned x, unsigned y, unsigned width, unsigned height);
	void (*clearColor)(float r, float g, float b, float a);
	void (*clearDepth)(float z);
	void (*clearStencil)(unsigned char s);

	void (*adjustDepthRangeMatrix)(float* inoutMat44);

// Render target
	void (*setDefaultFrameBuffer)(void* platformSpecificFrameBufferHandle);
	bool (*setRenderTargets)(roRDriverTexture** textures, roSize targetCount, bool useDepthStencil);

// State management
	void (*applyDefaultState)(roRDriverContext* self);
	void (*setBlendState)(roRDriverBlendState* blendState);
	void (*setRasterizerState)(roRDriverRasterizerState* rasterizerState);
	void (*setDepthStencilState)(roRDriverDepthStencilState* depthStencilState);
	void (*setTextureState)(roRDriverTextureState* textureStates, roSize stateCount, unsigned startingTextureUnit);

// Buffer
	roRDriverBuffer* (*newBuffer)();
	void (*deleteBuffer)(roRDriverBuffer* self);
	bool (*initBuffer)(roRDriverBuffer* self, roRDriverBufferType type, roRDriverDataUsage usage, void* initData, roSize sizeInBytes);
	bool (*updateBuffer)(roRDriverBuffer* self, roSize offsetInBytes, void* data, roSize sizeInBytes);
	void* (*mapBuffer)(roRDriverBuffer* self, roRDriverBufferMapUsage usage);
	void (*unmapBuffer)(roRDriverBuffer* self);

// Texture
	roRDriverTexture* (*newTexture)();
	void (*deleteTexture)(roRDriverTexture* self);
	bool (*initTexture)(roRDriverTexture* self, unsigned width, unsigned height, roRDriverTextureFormat format, roRDriverTextureFlag flags);	// Can be invoked in loader thread
	bool (*commitTexture)(roRDriverTexture* self, const void* data, roSize rowPaddingInBytes);	// Can only be invoked in render thread

// Shader
	roRDriverShader* (*newShader)();
	void (*deleteShader)(roRDriverShader* self);
	bool (*initShader)(roRDriverShader* self, roRDriverShaderType type, const char** sources, roSize sourceCount);

	bool (*bindShaders)(roRDriverShader** shaders, roSize shaderCount);
	bool (*setUniformTexture)(unsigned nameHash, roRDriverTexture* texture);

	bool (*bindShaderInput)(roRDriverShaderInput* inputs, roSize inputCount, unsigned* cacheId);	// Get the cacheId for the first call, and it speed up on later calls

// Making draw call
	void (*drawTriangle)(roSize offset, roSize vertexCount, unsigned flags);
	void (*drawTriangleIndexed)(roSize offset, roSize indexCount, unsigned flags);
	void (*drawPrimitive)(roRDriverPrimitiveType type, roSize offset, roSize vertexCount, unsigned flags);
	void (*drawPrimitiveIndexed)(roRDriverPrimitiveType type, roSize offset, roSize indexCount, unsigned flags);

// Driver version
	const char* driverName;
	unsigned majorVersion;
	unsigned minorVersion;
	void (*destructor)(roRDriver* self);
} roRDriver;

roRDriver* roNewRenderDriver(const char* driverType, const char* options);

void roDeleteRenderDriver(roRDriver* self);

extern roRDriverContext* roRDriverCurrentContext;

#ifdef __cplusplus
}
#endif

#endif	// __render_roRenderDriver_h__
