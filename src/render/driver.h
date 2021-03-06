#ifndef __RENDER_DRIVER_H__
#define __RENDER_DRIVER_H__

#include <stddef.h>
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

	static const unsigned maxTextureUnit = 2;

// Capability
	// Supported cap:
	// npot
	// maxtexsize
	static void* getCapability(const char* cap);

	static unsigned nextPowerOfTwo(unsigned x);
	static bool isPowerOfTwo(unsigned x);

// Render target
	static void* createRenderTargetExternal(void* externalHandle);

	/// @param textureHandle
	///		If null, no color buffer will be used.
	///		If *textureHandle is null, this function will create the texture automatically.
	///		If *textureHandle is not null, then the it's value will be used as the texture.
	/// @param depthStencilHandle
	///		If null, no depth and stencil buffer will be used.
	///		If *depthHandle is null, this function will create the depth and stencil automatically.
	///		If *depthHandle is not null, then the it's value will be used as the depth and stencil.
	static void* createRenderTarget(
		void* existingRenderTarget,
		void** textureHandle,
		void** depthHandle, void** stencilHandle,
		unsigned width, unsigned height
	);

	static void deleteRenderTarget(void* rtHandle, void** depthHandle, void** stencilHandle);

	static void useRenderTarget(void* rtHandle);

// Transformation
	static void getProjectionMatrix(float* matrix);
	static void setProjectionMatrix(const float* matrix);

	static void getViewMatrix(float* matrix);
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

	/// To use the texture, call setSamplerState()
	static void* createTexture(void* existingTexture, unsigned width, unsigned height, TextureFormat internalFormat=RGBA, const void* srcData=NULL, TextureFormat srcDataFormat=RGBA, unsigned packAlignment=1);
	static void deleteTexture(void* textureHandle);

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

// Vertex/Index buffer
	enum VertexFormat {
		P,
		P2f,	// Position with 2 float
		P_UV0,
		P_C,
		P_C_UV0,
		P_N_UV0_UV1,
	};

	static void beginVertex(VertexFormat format);
	static	rhuint16 vertex3f(float x, float y, float z);
	static	void normal3f(float x, float y, float z);
	static  void color4b(rhuint8 r, rhuint8 g, rhuint8 b, rhuint8 a);
	static	void texUnit(unsigned unit);
	static	void texCoord2f(float u, float v);
	static void* endVertex();

	static void* createVertexCopyData(VertexFormat format, const void* vertexData, unsigned vertexCount, unsigned stride);	// Free the data yourself
	static void* createVertexUseData(VertexFormat format, const void* vertexData, unsigned vertexCount, unsigned stride);	// Free the data yourself
	static void destroyVertex(void* vertexHandle);

	static void beginIndex();
	static  void addIndex(rhuint16 idx);
	static  void addIndex(rhuint16* idx, unsigned indexCount);
	static void* endIndex();

	static void* createIndexCopyData(const void* indexData, unsigned indexCount);	// Free the data yourself
	static void* createIndexUseData(const void* indexData, unsigned indexCount);	// Free the data yourself
	static void destroyIndex(void* indexHandle);

// Transformation
	static void pushMatrix();
	static void pophMatrix();

// Input assembler state
	struct InputAssemblerState
	{
		enum PrimitiveType {
			Triangles		= 0x0004,
			TriangleStrip	= 0x0005,
			TriangleFan		= 0x0006,
		};

		PrimitiveType primitiveType;
		void* vertexBuffer;
		void* indexBuffer;
	};	// InputAssemblerState

	static void setInputAssemblerState(const InputAssemblerState& state);

// Sampler state
	struct SamplerState
	{
		enum Filter {
			MIN_MAG_POINT	= 0,
			MIN_MAG_LINEAR	= 1,
			MIP_MAG_POINT	= 2,
			MIP_MAG_LINEAR	= 3,
		};

		enum AddressMode {
			Repeat			= 0x2901,
			Edge			= 0x812F,
			Border			= 0x812D,
			MirrorRepeat	= 0x8370,
		};

		void* textureHandle;
		Filter filter;
		AddressMode u, v;
	};	// SamplerState

	static void getSamplerState(unsigned textureUnit, SamplerState& state);
	static void setSamplerState(unsigned textureUnit, const SamplerState& state);

// Rasterizer state
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

	static void setRasterizerState(const RasterizerState& state);

// Depth stenchil state
	struct DepthStencilState
	{
		enum CompareFunc {
			Never		= 0x0200,	// Never pass, 
			Less		= 0x0201,	// Pass if src depth < dst depth, pass if (ref & mask) < (stencil & mask)
			Equal		= 0x0202,
			LEqual		= 0x0203,
			Greater		= 0x0204,
			NotEqual	= 0x0205,
			GEqual		= 0x0206,
			Always		= 0x0207,
		};

		bool depthEnable;
		CompareFunc depthFunc;

		bool stencilEnable;

		struct StencilState
		{
			enum StencilOp {
				Zero		= 0,
				Invert		= 0x150A,
				Keep		= 0x1E00,
				Replace		= 0x1E01,
				Incr		= 0x1E02,
				Decr		= 0x1E03,
				IncrWrap	= 0x8507,
				DecrWrap	= 0x8508,
			};

			rhuint8 stencilRefValue;
			rhuint8 stencilMask;
			CompareFunc stencilFunc;
			StencilOp stencilFailOp, stencilZFailOp, stencilPassOp;

			StencilState();
			StencilState(rhuint8 stencilRefValue, rhuint8 stencilMask, CompareFunc stencilFunc, StencilOp stencilOp);
			StencilState(rhuint8 stencilRefValue, rhuint8 stencilMask, CompareFunc stencilFunc, StencilOp stencilFailOp, StencilOp stencilZFailOp, StencilOp stencilPassOp);
		};

		StencilState stencilFront, stencilBack;

		DepthStencilState();
		DepthStencilState(bool depthEnable, CompareFunc depthFunc, bool stencilEnable, StencilState frontAndBack);
		DepthStencilState(bool depthEnable, CompareFunc depthFunc, bool stencilEnable, StencilState front, StencilState back);
	};	// DepthStencilState

	static void getDepthStencilState(DepthStencilState& state);
	static void setDepthStencilState(const DepthStencilState& state);
	static void setDepthTestEnable(bool b);
	static void setStencilTestEnable(bool b);

// Blend state
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

		enum ColorWriteEnable {
			DisableAll	= 0,
			EnableRed	= 1,
			EnableGreen	= 2,
			EnableBlue	= 4,
			EnableAlpha	= 8,
			EnableAll	= EnableRed | EnableGreen | EnableBlue | EnableAlpha,
		};

		bool enable;
		BlendOp colorOp, alphaOp;
		BlendValue colorSrc, colorDst, alphaSrc, alphaDst;
		ColorWriteEnable wirteMask;
	};	// BlendState

	static void getBlendState(BlendState& state);
	static void setBlendState(const BlendState& state);
	static void setBlendEnable(bool b);
	static void setColorWriteMask(BlendState::ColorWriteEnable mask);

// View port state
	struct ViewportState
	{
		unsigned left, top, width, height;
	};	// ViewportState

	static void getViewportState(ViewportState& state);
	static void setViewport(const ViewportState& state);
	static void setViewport(float left, float top, float width, float height);
	static void setViewport(unsigned left, unsigned top, unsigned width, unsigned height);

// Draw command
	// Draw using the binded vertex buffer
	static void draw(unsigned vertexCount, unsigned startingVertex=0);

	// Draw using the binded vertex and index buffers
	static void drawIndexed(unsigned indexCount, unsigned startingIndex=0);

// Pixel manipulation
	static void readPixels(unsigned x, unsigned y, unsigned width, unsigned height, TextureFormat format, unsigned char* data);

	static void writePixels(unsigned x, unsigned y, unsigned width, unsigned height, TextureFormat format, const unsigned char* data);
};	// Driver

class Material
{
public:
	virtual ~Material() {}
	virtual void preRender() = 0;
	virtual void postRender() = 0;
};	// Material

}	// Render

#endif	// __RENDER_DRIVER_H__
