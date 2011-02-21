#include "pch.h"
#include "../driver.h"
#include "../../common.h"
#include "../../Vec3.h"

#include <gl/gl.h>
#include "../gl/glext.h"

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

static bool glInited = false;

namespace Render {

void Driver::init()
{
	if(glInited)
		return;

	glInited = true;

	glBlendFuncSeparate = (PFNGLBLENDFUNCSEPARATEPROC) wglGetProcAddress("glBlendFuncSeparate");
	glBlendEquationSeparate = (PFNGLBLENDEQUATIONSEPARATEPROC)wglGetProcAddress("glBlendEquationSeparate");

	glBindFramebuffer = (PFNGLBINDFRAMEBUFFERPROC) wglGetProcAddress("glBindFramebuffer");
	glGenFramebuffers = (PFNGLGENFRAMEBUFFERSPROC) wglGetProcAddress("glGenFramebuffers");
	glDeleteFramebuffers = (PFNGLDELETEFRAMEBUFFERSPROC) wglGetProcAddress("glDeleteFramebuffers");

	glGenRenderbuffers = (PFNGLGENRENDERBUFFERSPROC) wglGetProcAddress("glGenRenderbuffers");
	glBindRenderbuffer = (PFNGLBINDRENDERBUFFERPROC) wglGetProcAddress("glBindRenderbuffer");
	glDeleteRenderbuffers = (PFNGLDELETERENDERBUFFERSPROC) wglGetProcAddress("glDeleteRenderbuffers");

	glRenderbufferStorage = (PFNGLRENDERBUFFERSTORAGEPROC) wglGetProcAddress("glRenderbufferStorage");
	glFramebufferRenderbuffer = (PFNGLFRAMEBUFFERRENDERBUFFERPROC) wglGetProcAddress("glFramebufferRenderbuffer");

	glFramebufferTexture2D = (PFNGLFRAMEBUFFERTEXTURE2DPROC) wglGetProcAddress("glFramebufferTexture2D");
	glCheckFramebufferStatus = (PFNGLCHECKFRAMEBUFFERSTATUSPROC) wglGetProcAddress("glCheckFramebufferStatus");
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
	bool enableTexture2D;
	void* renderTarget;
	void* texture;

	float vpLeft, vpTop, vpWidth, vpHeight;	// Viewport
	unsigned viewportHash;

	bool vertexArrayEnabled, coordArrayEnabled, colorArrayEnabled;

	Driver::BlendState blendState;
	unsigned blendStateHash;
};	// Context

static Context* _context = NULL;

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

	ctx->enableTexture2D = true;
	glDisable(GL_TEXTURE_2D);

	ctx->vpLeft = ctx->vpTop = 0;
	ctx->vpWidth = ctx->vpHeight = 0;
	ctx->viewportHash = 0;

	enableVertexArray(false, true, ctx);
	enableColorArray(false, true, ctx);
	enableCoordArray(false, true, ctx);

	ctx->blendState.enable = false;
	ctx->blendState.colorOp = BlendState::Add;
	ctx->blendState.alphaOp = BlendState::Add;
	ctx->blendState.colorSrc = BlendState::One;
	ctx->blendState.colorDst = BlendState::Zero;
	ctx->blendState.alphaSrc = BlendState::One;
	ctx->blendState.alphaDst = BlendState::Zero;

	glDisable(GL_BLEND);
	glBlendFuncSeparate(GL_ONE, GL_ZERO, GL_ONE, GL_ZERO);
	glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);

	ctx->renderTarget = NULL;
	ctx->texture = NULL;

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

void Driver::forceApplyCurrent()
{
	glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)_context->renderTarget);

	if(_context->enableTexture2D)
		glEnable(GL_TEXTURE_2D);
	else
		glDisable(GL_TEXTURE_2D);

	glBindTexture(GL_TEXTURE_2D, (GLuint)_context->texture);

	glViewport(
		(GLint)_context->vpLeft,
		(GLint)_context->vpTop,
		(GLsizei)_context->vpWidth,
		(GLsizei)_context->vpHeight
	);

	enableVertexArray(_context->vertexArrayEnabled, true);
	enableColorArray(_context->coordArrayEnabled, true);
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

void* Driver::createRenderTargetTexture(void** textureHandle, void** depthStencilHandle, unsigned width, unsigned height)
{
	ASSERT(GL_NO_ERROR == glGetError());

	GLuint handle;
	glGenFramebuffers(1, &handle);
	glBindFramebuffer(GL_FRAMEBUFFER, handle);

	if(textureHandle) {
		if(!*textureHandle)	// Generate the texture right here
			*textureHandle = createTexture(width, height);

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, (GLuint)*textureHandle, 0);
	}

	if(depthStencilHandle) {
		if(!*depthStencilHandle) {	// Generate the depth stencil right here
			glGenRenderbuffers(1, (GLuint*)depthStencilHandle);
			glBindRenderbuffer(GL_RENDERBUFFER, (GLuint)*depthStencilHandle);
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
		}

		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, (GLuint)*depthStencilHandle);
	}

	ASSERT(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
	ASSERT(GL_NO_ERROR == glGetError());

	glBindRenderbuffer(GL_RENDERBUFFER, 0);

	glViewport(0, 0, width, height);
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

void* Driver::createTexture(unsigned width, unsigned height, TextureFormat internalFormat, const void* srcData, TextureFormat srcDataFormat)
{
	ASSERT(GL_NO_ERROR == glGetError());

	if(internalFormat == ANY)
		internalFormat = autoChooseFormat(srcDataFormat);

	GLenum type = GL_TEXTURE_2D;
	GLuint handle;
	glGenTextures(1, &handle);
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
	glBindTexture(type, (GLuint)_context->texture);

	ASSERT(GL_NO_ERROR == glGetError());
	return reinterpret_cast<void*>(handle);
}

void Driver::deleteTexture(void* textureHandle)
{
	if(_context->texture == textureHandle)
		_context->texture = NULL;
	GLuint handle = reinterpret_cast<GLuint>(textureHandle);
	if(handle) glDeleteTextures(1, &handle);
}

void Driver::useTexture(void* textureHandle)
{
	if(_context->texture == textureHandle) return;
	_context->texture = textureHandle;

	GLuint handle = reinterpret_cast<GLuint>(textureHandle);

	if(handle) {
		if(!_context->enableTexture2D) {
			glEnable(GL_TEXTURE_2D);
			_context->enableTexture2D = true;
		}
	}
	else {
		if(_context->enableTexture2D) {
			glDisable(GL_TEXTURE_2D);
			_context->enableTexture2D = false;
		}
	}
	glBindTexture(GL_TEXTURE_2D, handle);
}

// Draw quad
void Driver::drawQuad(
	float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, float z,
	rhuint8 r, rhuint8 g, rhuint8 b, rhuint8 a
)
{
	Driver::useTexture(NULL);

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
void Driver::beginMesh(MeshFormat format)
{
}

void Driver::vertex3f(float x, float y, float z)
{
}

void Driver::normal3f(float x, float y, float z)
{
}

void Driver::texUnit(unsigned unit)
{
}

void Driver::texCoord2f(float u, float v)
{
}

int Driver::endMesh()
{
	return 0;
}

int Driver::createMeshCopyData(MeshFormat format, const float* vertexBuffer, const short* indexBuffer)
{
	return 0;
}

int Driver::createMeshUseData(MeshFormat format, const float* vertexBuffer, const short* indexBuffer)
{
	return 0;
}

void Driver::alertMeshDataChanged(int meshHandle)
{
}

void Driver::destroyMesh(int meshHandle)
{
}

void Driver::useMesh(int meshHandle)
{
}

// States
void Driver::setRasterizerState(const RasterizerState& state)
{
}

void Driver::setBlendState(const BlendState& state)
{
	unsigned h = hash(&state, sizeof(state));
	if(h == _context->blendStateHash) return;

	if(!state.enable) {
		glDisable(GL_BLEND);
	}
	else {
		glEnable(GL_BLEND);
		glBlendFuncSeparate(state.colorSrc, state.colorDst, state.alphaSrc, state.alphaDst);
		glBlendEquationSeparate(state.colorOp, state.alphaOp);
	}

	_context->blendState = state;
	_context->blendStateHash = h;
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
