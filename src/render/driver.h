#ifndef __RENDER_DRIVER_H__
#define __RENDER_DRIVER_H__

#include "../rhinoca.h"

namespace Render {

class Driver
{
public:
	static void init();
	static void close();

// Context
	static void* createContext(void* externalHandle);
	static void useContext(void* contextHandle);
	static void deleteContext(void* contextHandle);

	static void forceApplyCurrent();

// Capability
	// Supported cap:
	// npot
	static void* getCapability(const char* cap);

	static unsigned nextPowerOfTwo(unsigned x);

// Render target
	static void* createRenderTargetExternal(void* externalHandle);

	/// @param textureHandle
	///		If null, no color buffer will be used.
	///		If *textureHandle is null, this function will create the texture automatically.
	///		If *textureHandle is not null, then the it's value will be used as the texture.
	/// @param depthStencilHandle
	///		If null, no depth and stencil buffer will be used.
	///		If *depthStencilHandle is null, this function will create the depth and stencil automatically.
	///		If *depthStencilHandle is not null, then the it's value will be used as the depth and stencil.
	static void* createRenderTargetTexture(void** textureHandle, void** depthStencilHandle, unsigned width, unsigned height);

	static void deleteRenderTarget(void* rtHandle);

	static void useRenderTarget(void* rtHandle);

// Transformation
	static void setProjectionMatrix(const float* matrix);
	static void setViewMatrix(const float* matrix);

	static void ortho(float left, float right, float bottom, float top, float zNear, float zFar);
	static void ortho(unsigned left, unsigned right, unsigned bottom, unsigned top, unsigned zNear, unsigned zFar);

// Texture
	enum TextureFormat {
		ANY				= 0,
		LUMINANCE		= 0x1909,
		LUMINANCE_ALPHA	= 0x190A,
		RGB				= 0x1907,
		BGR				= 0x80E0,
		RGBA			= 0x1908,
		BGRA			= 0x80E1,
	};

	static void* createTexture(unsigned width, unsigned height, TextureFormat internalFormat=RGBA, const void* srcData=NULL, TextureFormat srcDataFormat=RGBA);
	static void deleteTexture(void* textureHandle);

	static void useTexture(void* textureHandle);

// Draw quad
	/// Specify the vertex in a "ring" order
	static void drawQuad(
		float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, float z,
		rhuint8 r, rhuint8 g, rhuint8 b, rhuint8 a
	);

	static void drawQuad(
		float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, float z,
		float r, float g, float b, float a
	);

	static void drawQuad(
		float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, float z,
		float u1, float v1, float u2, float v2, float u3, float v3, float u4, float v4,
		rhuint8 r, rhuint8 g, rhuint8 b, rhuint8 a
	);

// Mesh
	enum MeshFormat {
		P_UV0,
		P_C,
		P_C_UV0,
		P_N_UV0_UV1,
	};

	static void beginMesh(MeshFormat format);
	static	void vertex3f(float x, float y, float z);
	static	void normal3f(float x, float y, float z);
	static	void texUnit(unsigned unit);
	static	void texCoord2f(float u, float v);
	static int endMesh();

	static int createMeshCopyData(MeshFormat format, const float* vertexBuffer, const short* indexBuffer);

	static int createMeshUseData(MeshFormat format, const float* vertexBuffer, const short* indexBuffer);
	static void alertMeshDataChanged(int meshHandle);

	static void destroyMesh(int meshHandle);

	static void useMesh(int meshHandle);

// States
	struct RasterizerState
	{
		enum FillMode {
			Wireframe,
			Solid,
		};

		enum CullMode {
			None,
			Front,
			Back,
		};

		bool multisampleEnable;
		FillMode fillMode;
		CullMode cullMode;
	};	// RasterizerState

	struct BlendState
	{
		enum BlendValue {
			Zero		= 0,
			One			= 1,
			SrcColor	= 0x0300,
			InvSrcColor	= 0x0301,
			SrcAlpha	= 0x0302,
			InvSrcAlpha	= 0x0303,
			DstColor	= 0x0306,
			InvDstColor	= 0x0307,
			DstAlpha	= 0x0304,
			InvDstAlpha	= 0x0305,
		};

		enum BlendOp {
			Add			= 0x8006,
			Subtract	= 0x800A,
			RevSubtract	= 0x800B,
			Min			= 0x8007,
			Max			= 0x8008,
		};

		bool enable;
		BlendOp colorOp, alphaOp;
		BlendValue colorSrc, colorDst, alphaSrc, alphaDst;
	};	// BlendState

	static void setRasterizerState(const RasterizerState& state);
	static void setBlendState(const BlendState& state);
	static void setBlendEnable(bool b);

	static void setViewport(float left, float top, float width, float height);
	static void setViewport(unsigned left, unsigned top, unsigned width, unsigned height);
};	// Driver

}	// Render

#endif	// __RENDER_DRIVER_H__
