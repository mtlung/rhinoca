#include "pch.h"
#include "../driver.h"
#include "../driverdetail.h"
#include "../../common.h"
#include "../../Vec3.h"

#include <gl/gl.h>
#include "../gl/glext.h"

#pragma comment(lib, "OpenGL32")
#pragma comment(lib, "GLU32")

PFNGLBLENDFUNCSEPARATEPROC glBlendFuncSeparate = NULL;
PFNGLBLENDEQUATIONSEPARATEPROC glBlendEquationSeparate = NULL;

PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer = NULL;
PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers = NULL;
PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers = NULL;

PFNGLGENRENDERBUFFERSPROC glGenRenderbuffers = NULL;
PFNGLBINDRENDERBUFFERPROC glBindRenderbuffer = NULL;
PFNGLDELETERENDERBUFFERSPROC glDeleteRenderbuffers = NULL;

PFNGLRENDERBUFFERSTORAGEPROC glRenderbufferStorage = NULL;
PFNGLFRAMEBUFFERRENDERBUFFERPROC glFramebufferRenderbuffer = NULL;

PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D = NULL;
PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatus = NULL;

PFNGLACTIVETEXTUREPROC glActiveTexture = NULL;

static bool glInited = false;

namespace Render {

#define GET_FUNC_PTR(ptr, type) \
	if(!(ptr = (type) wglGetProcAddress(#ptr))) \
	{	ptr = (type) wglGetProcAddress(#ptr"EXT");	}

void Driver::init()
{
	if(glInited)
		return;

	glInited = true;

	GET_FUNC_PTR(glBlendFuncSeparate, PFNGLBLENDFUNCSEPARATEPROC);
	GET_FUNC_PTR(glBlendEquationSeparate, PFNGLBLENDEQUATIONSEPARATEPROC);
	GET_FUNC_PTR(glBindFramebuffer, PFNGLBINDFRAMEBUFFERPROC);
	GET_FUNC_PTR(glGenFramebuffers, PFNGLGENFRAMEBUFFERSPROC);
	GET_FUNC_PTR(glDeleteFramebuffers, PFNGLDELETEFRAMEBUFFERSPROC);
	GET_FUNC_PTR(glGenRenderbuffers, PFNGLGENRENDERBUFFERSPROC);	
	GET_FUNC_PTR(glBindRenderbuffer, PFNGLBINDRENDERBUFFERPROC);
	GET_FUNC_PTR(glDeleteRenderbuffers, PFNGLDELETERENDERBUFFERSPROC);
	GET_FUNC_PTR(glRenderbufferStorage, PFNGLRENDERBUFFERSTORAGEPROC);
	GET_FUNC_PTR(glFramebufferRenderbuffer, PFNGLFRAMEBUFFERRENDERBUFFERPROC);
	GET_FUNC_PTR(glFramebufferTexture2D, PFNGLFRAMEBUFFERTEXTURE2DPROC);
	GET_FUNC_PTR(glCheckFramebufferStatus, PFNGLCHECKFRAMEBUFFERSTATUSPROC);

	GET_FUNC_PTR(glActiveTexture, PFNGLACTIVETEXTUREPROC);
}

void Driver::close()
{
}

static unsigned hash(const void* data, unsigned len)
{
	unsigned h = 0;
	const char* data_ = reinterpret_cast<const char*>(data);
	for(unsigned i=0; i<len; ++i)
		h = data_[i] + (h << 6) + (h << 16) - h;
	return h;
}

class Context
{
public:
	void* renderTarget;

	float vpLeft, vpTop, vpWidth, vpHeight;	// Viewport
	unsigned viewportHash;

	bool vertexArrayEnabled, coordArrayEnabled, colorArrayEnabled;

	Driver::SamplerState samplerStates[Driver::maxTextureUnit];
	unsigned samplerStateHash[Driver::maxTextureUnit];
	unsigned currentSampler;

	Driver::DepthStencilState depthStencilState;
	unsigned depthStencilStateHash;

	Driver::BlendState blendState;
	unsigned blendStateHash;

	MeshBuilder meshBuilder;
};	// Context

static Context* _context = NULL;

MeshBuilder* currentMeshBuilder()
{
	return &_context->meshBuilder;
}

static void enableVertexArray(bool b, bool force=false, Context* c = _context)
{
	if(b && (force || !c->vertexArrayEnabled))
		glEnableClientState(GL_VERTEX_ARRAY);
	else if(!b && (force || c->vertexArrayEnabled))
		glDisableClientState(GL_VERTEX_ARRAY);

	c->vertexArrayEnabled = b;
}

static void enableColorArray(bool b, bool force=false, Context* c = _context)
{
	if(b && (force || !c->colorArrayEnabled))
		glEnableClientState(GL_COLOR_ARRAY);
	else if(!b && (force || c->colorArrayEnabled))
		glDisableClientState(GL_COLOR_ARRAY);

	c->colorArrayEnabled = b;
}

static void enableCoordArray(bool b, bool force=false, Context* c = _context)
{
	if(b && (force || !c->coordArrayEnabled))
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	else if(!b && (force || c->coordArrayEnabled))
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	c->coordArrayEnabled = b;
}

// Context
void* Driver::createContext(void* externalHandle)
{
	Context* ctx = new Context;

	ctx->vpLeft = ctx->vpTop = 0;
	ctx->vpWidth = ctx->vpHeight = 0;
	ctx->viewportHash = 0;

	ctx->vertexArrayEnabled = false;
	ctx->colorArrayEnabled = false;
	ctx->coordArrayEnabled = false;

	{	// Sampler
		ctx->currentSampler = 0;
		memset(ctx->samplerStates, 0, sizeof(ctx->samplerStates));
		memset(ctx->samplerStateHash, 0, sizeof(ctx->samplerStateHash));

		for(unsigned i=0; i<Driver::maxTextureUnit; ++i) {
			ctx->samplerStates[i].textureHandle = NULL;
			ctx->samplerStates[i].filter = SamplerState::MIN_MAG_POINT;
			ctx->samplerStates[i].u = SamplerState::Repeat;
			ctx->samplerStates[i].v = SamplerState::Repeat;
		}
	}

	{	// Depth stencil
		ctx->depthStencilState.depthEnable = false;
		ctx->depthStencilState.stencilEnable = false;
		ctx->depthStencilState.stencilRefValue = 0;
		ctx->depthStencilState.stencilMask = rhuint8(-1);
		ctx->depthStencilState.stencilFunc = DepthStencilState::Always;
		ctx->depthStencilState.stencilPassOp = DepthStencilState::Replace;
		ctx->depthStencilState.stencilFailOp = DepthStencilState::Replace;
		ctx->depthStencilState.stencilZFailOp = DepthStencilState::Replace;
		ctx->depthStencilStateHash = 0;
	}

	{	// Blending
		ctx->blendState.enable = false;
		ctx->blendState.colorOp = BlendState::Add;
		ctx->blendState.alphaOp = BlendState::Add;
		ctx->blendState.colorSrc = BlendState::One;
		ctx->blendState.colorDst = BlendState::Zero;
		ctx->blendState.alphaSrc = BlendState::One;
		ctx->blendState.alphaDst = BlendState::Zero;
		ctx->blendState.wirteMask = BlendState::EnableAll;
		ctx->blendStateHash = 0;
	}

	ctx->renderTarget = NULL;

	return ctx;
}

void Driver::useContext(void* ctx)
{
	_context = reinterpret_cast<Context*>(ctx);
}

void Driver::deleteContext(void* ctx)
{
	if(_context == ctx) _context = NULL;
	delete reinterpret_cast<Context*>(ctx);
}

static const int _magFilter[] = { GL_NEAREST, GL_LINEAR, GL_NEAREST, GL_LINEAR };
static const int _minFilter[] = { GL_NEAREST, GL_LINEAR, GL_NEAREST_MIPMAP_NEAREST, GL_LINEAR_MIPMAP_LINEAR };

void Driver::forceApplyCurrent()
{
	glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)_context->renderTarget);

	{	// Sampler states
		for(unsigned i=0; i<Driver::maxTextureUnit; ++i) {
			glActiveTexture(GL_TEXTURE0 + i);
			GLuint handle = reinterpret_cast<GLuint>(_context->samplerStates[i].textureHandle);

			if(handle)
			{
				glEnable(GL_TEXTURE_2D);
				glBindTexture(GL_TEXTURE_2D, handle);

				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, _context->samplerStates[i].u);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, _context->samplerStates[i].v);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, _magFilter[_context->samplerStates[i].filter]);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, _minFilter[_context->samplerStates[i].filter]);
			}
			else
				glDisable(GL_TEXTURE_2D);
		}

		glActiveTexture(GL_TEXTURE0 + _context->currentSampler);
	}

	glViewport(
		(GLint)_context->vpLeft,
		(GLint)_context->vpTop,
		(GLsizei)_context->vpWidth,
		(GLsizei)_context->vpHeight
	);

	enableVertexArray(_context->vertexArrayEnabled, true);
	enableColorArray(_context->colorArrayEnabled, true);
	enableCoordArray(_context->coordArrayEnabled, true);
}

// Capability
void* Driver::getCapability(const char* cap)
{
	return (void*)1;
}

// Render target
void* Driver::createRenderTargetExternal(void* externalHandle)
{
	return externalHandle;
}

void* Driver::createRenderTarget(void* existingRenderTarget, void** textureHandle, void** depthHandle, void** stencilHandle, unsigned width, unsigned height)
{
	ASSERT(GL_NO_ERROR == glGetError());

	GLuint handle;

	if(!existingRenderTarget)
		glGenFramebuffers(1, &handle);
	else
		handle = (GLuint)existingRenderTarget;

	glBindFramebuffer(GL_FRAMEBUFFER, handle);

	if(textureHandle) {
		// If textureHandle is null, it will generate the texture, otherwise the existing texture will be resized
		*textureHandle = createTexture(*textureHandle, width, height);

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, (GLuint)*textureHandle, 0);
	}

	if(depthHandle) {
		if(!*depthHandle) {	// Generate the depth stencil right here
			glGenRenderbuffers(1, (GLuint*)depthHandle);
		}

		glBindRenderbuffer(GL_RENDERBUFFER, (GLuint)*depthHandle);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, (GLuint)*depthHandle);
	}

	if(stencilHandle)
		*stencilHandle = *depthHandle;

	ASSERT(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
	ASSERT(GL_NO_ERROR == glGetError());

	glBindRenderbuffer(GL_RENDERBUFFER, 0);

	Driver::setViewport(0, 0, width, height);
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)_context->renderTarget);

	return reinterpret_cast<void*>(handle);
}

void Driver::deleteRenderTarget(void* rtHandle)
{
	if(_context->renderTarget == rtHandle)
		_context->renderTarget = NULL;
	GLuint handle = reinterpret_cast<GLuint>(rtHandle);
	if(handle) glDeleteFramebuffers(1, &handle);
}

void Driver::useRenderTarget(void* rtHandle)
{
	if(_context->renderTarget == rtHandle) return;
	_context->renderTarget = rtHandle;

	GLuint handle = reinterpret_cast<GLuint>(rtHandle);
	glBindFramebuffer(GL_FRAMEBUFFER, handle);
}

// Transformation
void Driver::setProjectionMatrix(const float* matrix)
{
	glMatrixMode(GL_PROJECTION);
	glLoadMatrixf(matrix);
}

void Driver::setViewMatrix(const float* matrix)
{
	glMatrixMode(GL_MODELVIEW);
	glLoadMatrixf(matrix);
}

// Texture
static Driver::TextureFormat autoChooseFormat(Driver::TextureFormat srcFormat)
{
	switch(srcFormat) {
	case Driver::BGR:
		return Driver::RGB;
	case Driver::BGRA:
		return Driver::RGBA;
	default:
		return srcFormat;
	}
}

void* Driver::createTexture(void* existingTexture, unsigned width, unsigned height, TextureFormat internalFormat, const void* srcData, TextureFormat srcDataFormat)
{
	ASSERT(GL_NO_ERROR == glGetError());

	if(internalFormat == ANY)
		internalFormat = autoChooseFormat(srcDataFormat);

	GLenum type = GL_TEXTURE_2D;
	GLuint handle;

	if(!existingTexture)
		glGenTextures(1, &handle);
	else
		handle = (GLuint)existingTexture;

	if(width != 0 && height != 0) {
		glBindTexture(type, handle);

		glTexParameteri(type, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(type, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glTexParameteri(type, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(type, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glTexImage2D(
			type, 0, internalFormat, width, height, 0,
			srcDataFormat,
			GL_UNSIGNED_BYTE,
			srcData
		);

		// Restore previous binded texture
		glBindTexture(type, (GLuint)_context->samplerStates[_context->currentSampler].textureHandle);
	}

	ASSERT(GL_NO_ERROR == glGetError());
	return reinterpret_cast<void*>(handle);
}

void Driver::deleteTexture(void* textureHandle)
{
	GLuint handle = (GLuint)textureHandle;
	if(handle)
		glDeleteTextures(1, &handle);

	for(unsigned i=0; i<Driver::maxTextureUnit; ++i) {
		if(_context->samplerStates[i].textureHandle == textureHandle)
			_context->samplerStates[i].textureHandle = 0;
	}
}

static const Driver::SamplerState noTexture = {
	NULL,
	Driver::SamplerState::MIN_MAG_LINEAR,
	Driver::SamplerState::Edge
};

// Draw quad
void Driver::drawQuad(
	float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, float z,
	rhuint8 r, rhuint8 g, rhuint8 b, rhuint8 a
)
{
	Driver::setSamplerState(0, noTexture);

	enableVertexArray(true);
	enableColorArray(true);
	enableCoordArray(false);

	rhuint8 rgba[] = { r,g,b,a, r,g,b,a, r,g,b,a, r,g,b,a };
	glColorPointer(4, GL_UNSIGNED_BYTE, 0, rgba);

	float xyz[] = { x1, y1, z, x2, y2, z, x3, y3, z, x4, y4, z };
	glVertexPointer(3, GL_FLOAT, 0, xyz);

	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

void Driver::drawQuad(
	float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, float z,
	float u1, float v1, float u2, float v2, float u3, float v3, float u4, float v4,
	rhuint8 r, rhuint8 g, rhuint8 b, rhuint8 a
)
{
	enableVertexArray(true);
	enableColorArray(true);
	enableCoordArray(true);

	rhuint8 rgba[] = { r,g,b,a, r,g,b,a, r,g,b,a, r,g,b,a };
	glColorPointer(4, GL_UNSIGNED_BYTE, 0, rgba);

	float uv[] = { u1, v1, u2, v2, u3, v3, u4, v4 };
	glTexCoordPointer(2, GL_FLOAT, 0, uv);

	float xyz[] = { x1, y1, z, x2, y2, z, x3, y3, z, x4, y4, z };
	glVertexPointer(3, GL_FLOAT, 0, xyz);

	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

// Mesh
void* Driver::endMesh()
{
	return 0;
}

void* Driver::createMeshCopyData(MeshFormat format, const void* vertexBuffer, const short* indexBuffer)
{
	return 0;
}

void* Driver::createMeshUseData(MeshFormat format, const void* vertexBuffer, const short* indexBuffer)
{
	return 0;
}

void Driver::destroyMesh(void* meshHandle)
{
}

void Driver::useMesh(void* meshHandle)
{
}

// States
void Driver::setSamplerState(unsigned textureUnit, const SamplerState& state)
{
	if(textureUnit != _context->currentSampler)
		glActiveTexture(GL_TEXTURE0 + textureUnit);

	_context->currentSampler = textureUnit;
	SamplerState& s = _context->samplerStates[textureUnit];

	// Bind the texture unit
	if(s.textureHandle != state.textureHandle)
	{
		GLuint handle = reinterpret_cast<GLuint>(state.textureHandle);

		if(handle) {
			if(!s.textureHandle)
				glEnable(GL_TEXTURE_2D);
		}
		else {
			if(s.textureHandle)
				glDisable(GL_TEXTURE_2D);
		}
		s.textureHandle = state.textureHandle;
		glBindTexture(GL_TEXTURE_2D, handle);
	}

	if(!state.textureHandle) return;

	// Setup sampler state
	unsigned h = hash(&state.filter, sizeof(state)-sizeof(void*));

	if(h == _context->samplerStateHash[textureUnit])
		return;

//	_context->samplerStateHash[textureUnit] = h;

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, state.u);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, state.v);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, _magFilter[state.filter]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, _minFilter[state.filter]);
}

void Driver::setRasterizerState(const RasterizerState& state)
{
}

void Driver::getDepthStencilState(DepthStencilState& state)
{
	state = _context->depthStencilState;
}

void Driver::setDepthStencilState(const DepthStencilState& state)
{
	unsigned h = hash(&state, sizeof(state));
	if(h == _context->depthStencilStateHash) return;

	if(!state.stencilEnable) {
		glDisable(GL_STENCIL_TEST);
	}
	else {
		glEnable(GL_STENCIL_TEST);
		glStencilFunc(
			state.stencilFunc,
			state.stencilRefValue,
			state.stencilMask
		);
		glStencilOp(state.stencilFailOp, state.stencilZFailOp, state.stencilPassOp);
	}

	_context->depthStencilState = state;
	_context->depthStencilStateHash = h;
}

void Driver::setDepthTestEnable(bool b)
{
	if(_context->depthStencilState.depthEnable == b)
		return;

	if(b) glEnable(GL_DEPTH_TEST);
	else glDisable(GL_DEPTH_TEST);

	_context->depthStencilState.depthEnable = b;
	_context->depthStencilStateHash = hash(&_context->depthStencilState, sizeof(DepthStencilState));
}

void Driver::setStencilTestEnable(bool b)
{
	if(_context->depthStencilState.stencilEnable == b)
		return;

	if(b) glEnable(GL_STENCIL_TEST);
	else glDisable(GL_STENCIL_TEST);

	_context->depthStencilState.stencilEnable = b;
	_context->depthStencilStateHash = hash(&_context->depthStencilState, sizeof(DepthStencilState));
}

void Driver::getBlendState(BlendState& state)
{
	state = _context->blendState;
}

// Only hash BlendOp and BlendValue
static unsigned blendStateHash(const Driver::BlendState& state)
{
	unsigned h = hash(
		&state.colorOp,
		offsetof(Driver::BlendState, Driver::BlendState::wirteMask) -
		offsetof(Driver::BlendState, Driver::BlendState::colorOp)
	);
	return h;
}

void Driver::setBlendState(const BlendState& state)
{
	setBlendEnable(state.enable);

	if(state.wirteMask != _context->blendState.wirteMask) {
		glColorMask(
			(state.wirteMask & BlendState::EnableRed) > 0,
			(state.wirteMask & BlendState::EnableGreen) > 0,
			(state.wirteMask & BlendState::EnableBlue) > 0,
			(state.wirteMask & BlendState::EnableAlpha) > 0
		);
	}

	unsigned h = blendStateHash(state);
	if(h != _context->blendStateHash) {
		glBlendFuncSeparate(state.colorSrc, state.colorDst, state.alphaSrc, state.alphaDst);
		glBlendEquationSeparate(state.colorOp, state.alphaOp);
	}

	_context->blendState = state;
	_context->blendStateHash = h;
}

void Driver::setBlendEnable(bool b)
{
	if(_context->blendState.enable == b)
		return;

	if(b) glEnable(GL_BLEND);
	else glDisable(GL_BLEND);

	_context->blendState.enable = b;
}

void Driver::setColorWriteMask(Driver::BlendState::ColorWriteEnable mask)
{
	if(mask == _context->blendState.wirteMask)
		return;

	_context->blendState.wirteMask = mask;

	glColorMask(
		(mask & BlendState::EnableRed) > 0,
		(mask & BlendState::EnableGreen) > 0,
		(mask & BlendState::EnableBlue) > 0,
		(mask & BlendState::EnableAlpha) > 0
	);
}

void Driver::setViewport(float left, float top, float width, float height)
{
	float data[] = { left, top, width, height };
	unsigned h = hash(data, sizeof(data));

	if(_context->viewportHash != h) {
		glViewport((GLint)left, (GLint)top, (GLsizei)width, (GLsizei)height);
		_context->viewportHash = h;

		_context->vpLeft = left;
		_context->vpTop = left;
		_context->vpWidth = width;
		_context->vpHeight = height;
	}
}

void Driver::setViewport(unsigned left, unsigned top, unsigned width, unsigned height)
{
	setViewport(float(left), float(top), float(width), float(height));
}

}	// Render
