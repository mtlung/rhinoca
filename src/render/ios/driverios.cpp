#include "../driver.h"
#include "../gl.h"
#include "../../common.h"
#include "../../Vec3.h"

namespace Render {

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
	
	Vec4 color;	/// Color only affect drawQuad
	unsigned colorHash;
	
	float vpLeft, vpTop, vpWidth, vpHeight;	// Viewport
	unsigned viewportHash;
	
	Driver::BlendState blendState;
	unsigned blendStateHash;
};	// Context

static Context* _currentContext = NULL;

// Context
void* Driver::createContext(void* externalHandle)
{
	Context* ctx = new Context;
	
	ctx->enableTexture2D = true;
	glEnable(GL_TEXTURE_2D);
	
	ctx->color = Vec4(1, 1, 1, 1);
	ctx->colorHash = hash(ctx->color.data, sizeof(ctx->color));
//	glColor4fv(ctx->color.data);
	
	ctx->vpLeft = ctx->vpTop = 0;
	ctx->vpWidth = ctx->vpHeight = 0;
	ctx->viewportHash = 0;
	
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
	_currentContext = reinterpret_cast<Context*>(ctx);
}

void Driver::deleteContext(void* ctx)
{
	if(_currentContext == ctx) _currentContext = NULL;
	delete reinterpret_cast<Context*>(ctx);
}

void Driver::forceApplyCurrent()
{
	glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)_currentContext->renderTarget);
	
	if(_currentContext->enableTexture2D)
		glEnable(GL_TEXTURE_2D);
	else
		glDisable(GL_TEXTURE_2D);
	
	glBindTexture(GL_TEXTURE_2D, (GLuint)_currentContext->texture);
	
//	glColor4fv(_currentContext->color.data);
	
	glViewport(
		(GLint)_currentContext->vpLeft,
		(GLint)_currentContext->vpTop,
		(GLsizei)_currentContext->vpWidth,
		(GLsizei)_currentContext->vpHeight
	);
	
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnableClientState(GL_VERTEX_ARRAY);
}

// Render target
void* Driver::createRenderTargetExternal(void* externalHandle)
{
	return externalHandle;
}

void* Driver::createRenderTargetTexture(void* textureHandle)
{
	ASSERT(GL_NO_ERROR == glGetError());
	
	GLuint handle;
	glGenFramebuffers(1, &handle);
	glBindFramebuffer(GL_FRAMEBUFFER, handle);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, (GLuint)textureHandle, 0);
	
	ASSERT(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
	ASSERT(GL_NO_ERROR == glGetError());
	
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	
	return reinterpret_cast<void*>(handle);
}

void Driver::deleteRenderTarget(void* rtHandle)
{
	if(_currentContext->renderTarget == rtHandle)
		_currentContext->renderTarget = NULL;
	GLuint handle = reinterpret_cast<GLuint>(rtHandle);
	if(handle) glDeleteFramebuffers(1, &handle);
}

void Driver::useRenderTarget(void* rtHandle)
{
	if(_currentContext->renderTarget == rtHandle) return;
	_currentContext->renderTarget = rtHandle;
	
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
	
	// In IOS, non power of 2 texture clamp to edge must be used
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
	glBindTexture(type, (GLuint)_currentContext->texture);
	
	ASSERT(GL_NO_ERROR == glGetError());
	return reinterpret_cast<void*>(handle);
}

void Driver::deleteTexture(void* textureHandle)
{
	if(_currentContext->texture == textureHandle)
		_currentContext->texture = NULL;
	GLuint handle = reinterpret_cast<GLuint>(textureHandle);
	if(handle) glDeleteTextures(1, &handle);
}

void Driver::useTexture(void* textureHandle)
{
	if(_currentContext->texture == textureHandle) return;
	_currentContext->texture = textureHandle;
	
	GLuint handle = reinterpret_cast<GLuint>(textureHandle);
	
	if(handle) {
		if(!_currentContext->enableTexture2D) {
			glEnable(GL_TEXTURE_2D);
			_currentContext->enableTexture2D = true;
		}
	}
	else {
		if(_currentContext->enableTexture2D) {
			glDisable(GL_TEXTURE_2D);
			_currentContext->enableTexture2D = false;
		}
	}
	glBindTexture(GL_TEXTURE_2D, handle);
}

// Color
void Driver::setColor(float r, float g, float b, float a)
{
	Vec4 c(r, g, b, a);
	unsigned h = hash(c.data, sizeof(c));
	
	if(_currentContext->colorHash != h) {
//		glColor4fv(c.data);
		
		_currentContext->colorHash = h;
		_currentContext->color = c;
	}
}

// Draw quad
void Driver::drawQuad(
	float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, float z
)
{
	Driver::useTexture(NULL);
	
	float xyz[] = { x1, y1, z, x2, y2, z, x3, y3, z, x4, y4, z };
	glVertexPointer(3, GL_FLOAT, 0, xyz);
	
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

void Driver::drawQuad(
	float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, float z,
	float u1, float v1, float u2, float v2, float u3, float v3, float u4, float v4
)
{
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
	if(h == _currentContext->blendStateHash) return;
	
	if(!state.enable) {
		glDisable(GL_BLEND);
	}
	else {
		glEnable(GL_BLEND);
		glBlendFuncSeparate(state.colorSrc, state.colorDst, state.alphaSrc, state.alphaDst);
		glBlendEquationSeparate(state.colorOp, state.alphaOp);
	}
	
	_currentContext->blendState = state;
	_currentContext->blendStateHash = h;
}

void Driver::setViewport(float left, float top, float width, float height)
{
	float data[] = { left, top, width, height };
	unsigned h = hash(data, sizeof(data));
	
	if(_currentContext->viewportHash != h) {
		glViewport((GLint)left, (GLint)top, (GLsizei)width, (GLsizei)height);
		_currentContext->viewportHash = h;
		
		_currentContext->vpLeft = left;
		_currentContext->vpTop = left;
		_currentContext->vpWidth = width;
		_currentContext->vpHeight = height;
	}
}

void Driver::setViewport(unsigned left, unsigned top, unsigned width, unsigned height)
{
	setViewport(float(left), float(top), float(width), float(height));
}

}	// Render
