#include "../driver.h"
#include "../driverdetail.h"
#include "../../common.h"
#include "../../Vec3.h"
#include <string.h>

#include "../gl.h"


// Reference:
// Optimization guid: http://iphone-3d-programming.labs.oreilly.com/ch09.html

namespace Render {

static const float _mat44Identity[16] = {
	1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1
};

void Driver::init()
{
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

	Driver::ViewportState viewportState;
	unsigned viewportHash;

	float projectionMatrix[16];

	bool vertexArrayEnabled, coordArrayEnabled, colorArrayEnabled, normalArrayEnabled;

	Driver::InputAssemblerState inputAssemblerState;
	GLuint currentVertexBuffer, currentIndexBuffer;	// May be pointer to memory or ogl handle

	Driver::SamplerState samplerStates[Driver::maxTextureUnit];
	unsigned samplerStateHash[Driver::maxTextureUnit];
	unsigned currentSampler;

	Driver::DepthStencilState depthStencilState;
	unsigned depthStencilStateHash;

	Driver::BlendState blendState;
	unsigned blendStateHash;

	bool supportNPOT;
	bool supportSeperateBlend;
	GLint maxTextureSize;

	struct OglArrayState {
		GLvoid* ptrOrHandle;
		unsigned size, stride;
	};

	OglArrayState vertexArrayState;
	OglArrayState colorArrayState;
	OglArrayState coordArrayState0;

	BufferBuilder bufferBuilder;
};	// Context

static Context* _context = NULL;

BufferBuilder* currentBufferBuilder()
{
	return &_context->bufferBuilder;
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

static void enableNormalArray(bool b, bool force=false, Context* c = _context)
{
	if(b && (force || !c->normalArrayEnabled))
		glEnableClientState(GL_NORMAL_ARRAY);
	else if(!b && (force || c->normalArrayEnabled))
		glDisableClientState(GL_NORMAL_ARRAY);

	c->normalArrayEnabled = b;
}

static void bindVertexBuffer(GLuint vb)
{
	if(_context->currentVertexBuffer != vb) {
		glBindBuffer(GL_ARRAY_BUFFER, vb);
		_context->currentVertexBuffer = vb;
	}
}

static void bindIndexBuffer(GLuint ib)
{
	if(_context->currentIndexBuffer != ib) {
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ib);
		_context->currentIndexBuffer = ib;
	}
}

// Context
void* Driver::createContext(void* externalHandle)
{
	Context* ctx = new Context;

	const char* extensions = (char*)glGetString(GL_EXTENSIONS);
	ctx->supportNPOT = strstr(extensions, "GL_APPLE_texture_2D_limited_npot") != 0;
	ctx->supportSeperateBlend = (
		strstr(extensions, "GL_OES_blend_equation_separate") != 0 &&
		strstr(extensions, "GL_OES_blend_func_separate") != 0);

	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &ctx->maxTextureSize);

	ctx->viewportState.left = ctx->viewportState.top = 0;
	ctx->viewportState.width = ctx->viewportState.height = 0;
	ctx->viewportHash = 0;

	{	float tmp[] = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
		memcpy(ctx->projectionMatrix, tmp, sizeof(tmp));
	}

	ctx->vertexArrayEnabled = false;
	ctx->colorArrayEnabled = false;
	ctx->coordArrayEnabled = false;
	ctx->normalArrayEnabled = false;

	{	// Input assembler
		ctx->inputAssemblerState.vertexBuffer = NULL;
		ctx->inputAssemblerState.indexBuffer = NULL;
		ctx->inputAssemblerState.primitiveType = Driver::InputAssemblerState::Triangles;
		ctx->currentVertexBuffer = 0;
		ctx->currentIndexBuffer = 0;
	}

	{	// Sampler
		ctx->currentSampler = 0;
		memset(ctx->samplerStates, 0, sizeof(ctx->samplerStates));
		memset(ctx->samplerStateHash, 0, sizeof(ctx->samplerStateHash));

		for(unsigned i=0; i<Driver::maxTextureUnit; ++i) {
			ctx->samplerStates[i].textureHandle = NULL;
			ctx->samplerStates[i].filter = SamplerState::MIP_MAG_LINEAR;
			ctx->samplerStates[i].u = SamplerState::Repeat;
			ctx->samplerStates[i].v = SamplerState::Repeat;
		}
	}

	{	// Depth stencil
		ctx->depthStencilState.depthEnable = false;
		ctx->depthStencilState.stencilEnable = false;
		ctx->depthStencilState.stencilFront = Driver::DepthStencilState::StencilState(rhuint8(0), rhuint8(-1), DepthStencilState::Always, DepthStencilState::StencilState::Replace);
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

	{	// Vertex buffer state
		Context::OglArrayState state = { NULL, 0, 0 };
		ctx->vertexArrayState = state;
		ctx->colorArrayState = state;
		ctx->coordArrayState0 = state;
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
	ASSERT(GL_NO_ERROR == glGetError());

	glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)_context->renderTarget);

	{	// Input assembler
		VertexBuffer* vb = reinterpret_cast<VertexBuffer*>(_context->inputAssemblerState.indexBuffer);
		GLuint hvb = vb ? (GLuint)vb->handle | (GLuint)vb->data: 0;
		glBindBuffer(GL_ARRAY_BUFFER, hvb);
		_context->currentVertexBuffer = hvb;

		IndexBuffer* ib = reinterpret_cast<IndexBuffer*>(_context->inputAssemblerState.indexBuffer);
		GLuint hib = ib ? (GLuint)ib->handle | (GLuint)ib->data: 0;
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, hib);
		_context->currentIndexBuffer = hib;

		enableVertexArray(_context->vertexArrayEnabled, true);
		enableColorArray(_context->colorArrayEnabled, true);
		enableCoordArray(_context->coordArrayEnabled, true);
		enableNormalArray(_context->normalArrayEnabled, true);

		Context::OglArrayState* arrState = &_context->colorArrayState;
		if(arrState->size) glColorPointer(arrState->size, GL_UNSIGNED_BYTE, arrState->stride, arrState->ptrOrHandle);

		arrState = &_context->coordArrayState0;
		glClientActiveTexture(GL_TEXTURE0);
		if(arrState->size) glTexCoordPointer(arrState->size, GL_FLOAT, arrState->stride, arrState->ptrOrHandle);

		arrState = &_context->vertexArrayState;
		if(arrState->size) glVertexPointer(arrState->size, GL_FLOAT, arrState->stride, arrState->ptrOrHandle);
	}

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
		(GLint)_context->viewportState.left,
		(GLint)_context->viewportState.top,
		(GLsizei)_context->viewportState.width,
		(GLsizei)_context->viewportState.height
	);

	glMatrixMode(GL_PROJECTION);
	glLoadMatrixf(_context->projectionMatrix);
	glMatrixMode(GL_MODELVIEW);

	ASSERT(GL_NO_ERROR == glGetError());
}

// Capability
void* Driver::getCapability(const char* cap)
{
	if(strcasecmp(cap, "npot") == 0)
		return _context->supportNPOT ? (void*)1 : NULL;
	if(strcasecmp(cap, "maxtexsize") == 0)
		return (void*)_context->maxTextureSize;
	return NULL;
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
		if(!*depthHandle)	// Generate the depth stencil right here
			glGenRenderbuffers(1, (GLuint*)depthHandle);

		// NOTE: Only iDevice 3G or later support FBO with stencil
		// See also http://lists.apple.com/archives/mac-opengl/2008/Aug/msg00091.html
		// http://happyfire.appspot.com/
		glBindRenderbuffer(GL_RENDERBUFFER, (GLuint)*depthHandle);
		glRenderbufferStorage(GL_RENDERBUFFER, TARGET_IPHONE_SIMULATOR ? GL_DEPTH_COMPONENT16 : GL_DEPTH24_STENCIL8_OES, width, height);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, (GLuint)*depthHandle);
	}

#if !TARGET_IPHONE_SIMULATOR
	// NOTE: iPhone simulator didn't support stencil!
	if(stencilHandle && depthHandle) {
		*stencilHandle = *depthHandle;
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, (GLuint)*stencilHandle);
	}
#endif

#ifndef NDEBUG
	// See http://www.khronos.org/opengles/sdk/docs/man/xhtml/glCheckFramebufferStatus.xml
	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	ASSERT(status != GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT);
	ASSERT(status != GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT);
	ASSERT(status != GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS);
	ASSERT(status != GL_FRAMEBUFFER_UNSUPPORTED && "Most likely stencil not supported");
	ASSERT(status == GL_FRAMEBUFFER_COMPLETE);
	ASSERT(GL_NO_ERROR == glGetError());
#endif

	glBindRenderbuffer(GL_RENDERBUFFER, 0);

	Driver::setViewport(0, 0, width, height);
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)_context->renderTarget);

	return reinterpret_cast<void*>(handle);
}

void Driver::deleteRenderTarget(void* rtHandle, void** depthHandle, void** stencilHandle)
{
	if(_context->renderTarget == rtHandle)
		_context->renderTarget = NULL;
	GLuint handle = reinterpret_cast<GLuint>(rtHandle);
	if(handle) glDeleteFramebuffers(1, &handle);

	// NOTE: The handle value of depth and stencil may be the same
	if(depthHandle && *depthHandle) if(!stencilHandle || *stencilHandle != *depthHandle) {
		glDeleteRenderbuffers(1, (GLuint*)depthHandle);
		*depthHandle = 0;
	}

	if(stencilHandle && *stencilHandle) {
		glDeleteRenderbuffers(1, (GLuint*)stencilHandle);
		*stencilHandle = 0;
	}
}

void Driver::useRenderTarget(void* rtHandle)
{
	if(_context->renderTarget == rtHandle) return;
	_context->renderTarget = rtHandle;

	GLuint handle = reinterpret_cast<GLuint>(rtHandle);
	glBindFramebuffer(GL_FRAMEBUFFER, handle);
}

// Transformation
void Driver::getProjectionMatrix(float* matrix)
{
	memcpy(matrix, _context->projectionMatrix, sizeof(_context->projectionMatrix));
}

void Driver::setProjectionMatrix(const float* matrix)
{
	bool isDirty = (memcmp(matrix, _context->projectionMatrix, sizeof(_context->projectionMatrix)) != 0);
	memcpy(_context->projectionMatrix, matrix, sizeof(_context->projectionMatrix));

	if(isDirty) {
		glMatrixMode(GL_PROJECTION);
		glLoadMatrixf(matrix);
		glMatrixMode(GL_MODELVIEW);
	}
}

void Driver::setViewMatrix(const float* matrix)
{
#ifndef NDEBUG
	int i;
	glGetIntegerv(GL_MATRIX_MODE, &i);
	ASSERT(i == GL_MODELVIEW);
#endif

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

	const GLint maxTexSize = _context->maxTextureSize;
	if(width > maxTexSize || height > maxTexSize) {
		printf("Driver::createTexture() - texture size w:%d, h:%d exceed the limit %d\n", width, height, maxTexSize);
		return NULL;
	}

	if(internalFormat == ANY)
		internalFormat = autoChooseFormat(srcDataFormat);

	unsigned texWidth = width;
	unsigned texHeight = height;

	if(!_context->supportNPOT) {
		texWidth = nextPowerOfTwo(texWidth);
		texHeight = nextPowerOfTwo(texHeight);
	}

	GLenum type = GL_TEXTURE_2D;
	GLuint handle;

	if(!existingTexture)
		glGenTextures(1, &handle);
	else
		handle = (GLuint)existingTexture;

	if(width != 0 && height != 0) {
		Driver::SamplerState backupState;
		Driver::getSamplerState(0, backupState);

		// In IOS, non power of 2 texture clamp to edge must be used
		Driver::SamplerState state = {
			(void*)handle,
			Driver::SamplerState::MIN_MAG_LINEAR,
			Driver::SamplerState::Edge,
			Driver::SamplerState::Edge,
		};
		Driver::setSamplerState(0, state);

		glTexImage2D(
			type, 0, internalFormat, texWidth, texHeight, 0,
			srcDataFormat,
			GL_UNSIGNED_BYTE,
			srcData
		);

		if(!_context->supportNPOT) {
			int area[] = { 0, 0, width, height };
			glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_CROP_RECT_OES, area);
		}

		// Restore previous state
		Driver::setSamplerState(0, backupState);
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
	Driver::SamplerState::Edge,
	Driver::SamplerState::Edge
};

// Draw quad
void Driver::drawQuad(
	float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, float z,
	rhuint8 r, rhuint8 g, rhuint8 b, rhuint8 a
)
{
	Driver::setSamplerState(0, noTexture);

	struct Vertex { float x,y,z; rhuint8 r,g,b,a; } vertex[] = {
		{ x1,y1,z, r,g,b,a },
		{ x2,y2,z, r,g,b,a },
		{ x3,y3,z, r,g,b,a },
		{ x4,y4,z, r,g,b,a }
	};
	void* vb = Driver::createVertexUseData(Driver::P_C, vertex, 4, sizeof(Vertex));

	InputAssemblerState state = { Driver::InputAssemblerState::TriangleFan, vb, NULL };
	Driver::setInputAssemblerState(state);
	Driver::draw(4, 0);

	Driver::destroyVertex(vb);
}

void Driver::drawQuad(
	float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, float z,
	float u1, float v1, float u2, float v2, float u3, float v3, float u4, float v4,
	rhuint8 r, rhuint8 g, rhuint8 b, rhuint8 a
)
{
	struct Vertex { float x,y,z; rhuint8 r,g,b,a; float u,v; } vertex[] = {
		{ x1,y1,z, r,g,b,a, u1,v1 },
		{ x2,y2,z, r,g,b,a, u2,v2 },
		{ x3,y3,z, r,g,b,a, u3,v3 },
		{ x4,y4,z, r,g,b,a, u4,v4 }
	};
	void* vb = Driver::createVertexUseData(Driver::P_C_UV0, vertex, 4, sizeof(Vertex));

	InputAssemblerState state = { Driver::InputAssemblerState::TriangleFan, vb, NULL };
	Driver::setInputAssemblerState(state);
	Driver::draw(4, 0);

	Driver::destroyVertex(vb);
}

// Vertex buffer
void* Driver::createVertexCopyData(VertexFormat format, const void* vertexData, unsigned vertexCount, unsigned stride)
{
	VertexBuffer* vb = new VertexBuffer;
	vb->format = format;
	vb->count = vertexCount;
	vb->data = NULL;
	vb->stride = stride;

	glGenBuffers(1, (GLuint*)&vb->handle);
	bindVertexBuffer((GLuint)vb->handle);
	glBufferData(GL_ARRAY_BUFFER, vertexSizeForFormat(format) * vertexCount, vertexData, GL_STATIC_DRAW);

	return vb;
}

void* Driver::createVertexUseData(VertexFormat format, const void* vertexData, unsigned vertexCount, unsigned stride)
{
	VertexBuffer* vb = new VertexBuffer;
	vb->format = format;
	vb->count = vertexCount;
	vb->data = (void*)vertexData;
	vb->stride = stride;
	vb->handle = 0;
	return vb;
}

// TODO: Implements object pool
void Driver::destroyVertex(void* vertexHandle)
{
	bindVertexBuffer(0);

	VertexBuffer* vb = reinterpret_cast<VertexBuffer*>(vertexHandle);

	// The external data should free by user
	// rhinoca_free(vb->data);

	if(vb->handle)
		glDeleteBuffers(1, (GLuint*)&vb->handle);

	delete vb;
}

// Index buffer
void* Driver::createIndexCopyData(const void* indexData, unsigned indexCount)
{
	IndexBuffer* ib = new IndexBuffer;
	ib->count = indexCount;
	ib->data = NULL;

	glGenBuffers(1, (GLuint*)&ib->handle);
	bindIndexBuffer((GLuint)ib->handle);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(rhuint16) * indexCount, indexData, GL_STATIC_DRAW);

	return ib;
}

void* Driver::createIndexUseData(const void* indexData, unsigned indexCount)
{
	IndexBuffer* ib = new IndexBuffer;
	ib->count = indexCount;
	ib->data = (void*)indexData;
	ib->handle = 0;
	return ib;
}

void Driver::destroyIndex(void* indexHandle)
{
	bindIndexBuffer(0);

	IndexBuffer* ib = reinterpret_cast<IndexBuffer*>(indexHandle);

	// The external data should free by user
	// rhinoca_free(ib->data);

	glDeleteBuffers(1, (GLuint*)&ib->handle);

	delete ib;
}

void Driver::setInputAssemblerState(const InputAssemblerState& state)
{
	VertexBuffer* vb = reinterpret_cast<VertexBuffer*>(state.vertexBuffer);
	unsigned offset = 0;

	{	// Assign color
		switch(vb->format) {
		case P_C:
		case P_C_UV0:
		{
			enableColorArray(true);
			offset = 3 * sizeof(float);
			if(vb->data) offset += unsigned(vb->data);

			Context::OglArrayState state = { (GLvoid*)offset, 4, vb->stride };
			if(memcmp(&state, &_context->colorArrayState, sizeof(state)) != 0) {
				glColorPointer(state.size, GL_UNSIGNED_BYTE, state.stride, state.ptrOrHandle);
				_context->colorArrayState = state;
			}
			break;
		}
		default:
			enableColorArray(false);
		}
	}

	{	// Assign tex coord
		switch(vb->format) {
		case P_C_UV0:
		{
			enableCoordArray(true);
			offset = 3 * sizeof(float) + 4;
			if(vb->data) offset += unsigned(vb->data);
//			glClientActiveTexture(GL_TEXTURE0);

			Context::OglArrayState state = { (GLvoid*)offset, 2, vb->stride };
			if(memcmp(&state, &_context->coordArrayState0, sizeof(state)) != 0) {
				glTexCoordPointer(state.size, GL_FLOAT, state.stride, state.ptrOrHandle);
				_context->coordArrayState0 = state;
			}
			break;
		}
		default:
			enableCoordArray(false);
		}
	}

	{	// Assign normal
		switch(vb->format) {
		case P_N_UV0_UV1:
			enableNormalArray(true);
			offset = 3 * sizeof(float);
			if(vb->data) offset += unsigned(vb->data);
			glNormalPointer(GL_FLOAT, vb->stride, (GLvoid*)offset);
			break;
		default:
			enableNormalArray(false);
		}
	}

	{	// Assign position
		unsigned fCount = vb->format == P2f ? 2 : 3;
		GLuint hvb = (GLuint)vb->data |	(GLuint)vb->handle;
		enableVertexArray(true);

		void* ptrOrHandle = vb->data ? vb->data : NULL;

		Context::OglArrayState state = { ptrOrHandle, fCount, vb->stride };
		if(memcmp(&state, &_context->vertexArrayState, sizeof(state)) != 0) {
			bindVertexBuffer(vb->data ? 0 : hvb);
			glVertexPointer(state.size, GL_FLOAT, state.stride, state.ptrOrHandle);
			_context->vertexArrayState = state;
		}
	}

	if(IndexBuffer* ib = reinterpret_cast<IndexBuffer*>(state.indexBuffer)) {
		if(ib->handle)
			bindIndexBuffer((GLuint)ib->handle);
		else
			bindIndexBuffer(0);
	}

	_context->inputAssemblerState = state;
}

void Driver::getSamplerState(unsigned textureUnit, SamplerState& state)
{
	state = _context->samplerStates[textureUnit];
}

void Driver::setSamplerState(unsigned textureUnit, const SamplerState& state)
{
	if(textureUnit != _context->currentSampler)
		glActiveTexture(GL_TEXTURE0 + textureUnit);

	_context->currentSampler = textureUnit;
	SamplerState& s = _context->samplerStates[textureUnit];

	bool textureChanged = false;

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

		// Whenever there is a texture switch, we need to set the sampler state
		textureChanged = true;
		s.textureHandle = state.textureHandle;
		glBindTexture(GL_TEXTURE_2D, handle);
	}

	// Setup sampler state
	unsigned h = hash(&state.filter, sizeof(state)-sizeof(void*));

	if(!textureChanged && h == _context->samplerStateHash[textureUnit]) {
#ifndef NDEBUG
		if(state.textureHandle) {
			GLint magFilter;
			glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &magFilter);

			ASSERT(_magFilter[state.filter] == magFilter);
		}
#endif
		return;
	}

	_context->samplerStateHash[textureUnit] = h;
	_context->samplerStates[textureUnit] = state;

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
			state.stencilFront.stencilFunc,
			state.stencilFront.stencilRefValue,
			state.stencilFront.stencilMask
		);
		glStencilOp(state.stencilFront.stencilFailOp, state.stencilFront.stencilZFailOp, state.stencilFront.stencilPassOp);
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
		offsetof(Driver::BlendState, wirteMask) -
		offsetof(Driver::BlendState, colorOp)
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
		if(_context->supportSeperateBlend) {
			glBlendFuncSeparate(state.colorSrc, state.colorDst, state.alphaSrc, state.alphaDst);
			glBlendEquationSeparate(state.colorOp, state.alphaOp);
		} else {
			glBlendFunc(state.colorSrc, state.colorDst);
			glBlendEquation(state.colorOp);
		}
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

void Driver::getViewportState(ViewportState& state)
{
	state = _context->viewportState;
}

void Driver::setViewport(const ViewportState& state)
{
	unsigned h = hash(&state, sizeof(state));

	if(_context->viewportHash != h) {
		glViewport((GLint)state.left, (GLint)state.top, (GLsizei)state.width, (GLsizei)state.height);
		_context->viewportHash = h;
		_context->viewportState = state;
	}
}

void Driver::setViewport(float left, float top, float width, float height)
{
	ViewportState state = { (unsigned)left, (unsigned)top, (unsigned)width, (unsigned)height };
	setViewport(state);
}

void Driver::setViewport(unsigned left, unsigned top, unsigned width, unsigned height)
{
	setViewport(float(left), float(top), float(width), float(height));
}

void Driver::draw(unsigned vertexCount, unsigned startingVertex)
{
	InputAssemblerState& state = _context->inputAssemblerState;
	VertexBuffer* vb = reinterpret_cast<VertexBuffer*>(state.vertexBuffer);

	ASSERT(vertexCount <= vb->count);
	glDrawArrays(state.primitiveType, startingVertex, vertexCount);
}

void Driver::drawIndexed(unsigned indexCount, unsigned startingIndex)
{
	InputAssemblerState& state = _context->inputAssemblerState;
	IndexBuffer* ib = reinterpret_cast<IndexBuffer*>(state.indexBuffer);

	if(ib->handle)
		glDrawElements(state.primitiveType, indexCount, GL_UNSIGNED_SHORT, (GLvoid*)startingIndex);
	else if(ib->data)
		glDrawElements(state.primitiveType, indexCount, GL_UNSIGNED_SHORT, ((rhuint16*)ib->data) + startingIndex);
}

void Driver::readPixels(unsigned x, unsigned y, unsigned width, unsigned height, TextureFormat format, unsigned char* data)
{
	glPixelStorei(GL_PACK_ALIGNMENT,1);
//	GLint lastBuffer;
//	glGetIntegerv(GL_READ_BUFFER, &lastBuffer);
	glReadPixels((GLint)x, (GLint)y, (GLsizei)width, (GLsizei)height, (GLenum)format, GL_UNSIGNED_BYTE, data);
//	glReadBuffer(lastBuffer);
}

void Driver::writePixels(unsigned x, unsigned y, unsigned width, unsigned height, TextureFormat format, const unsigned char* data)
{
	// OpenGL ES didn't support glDrawPixels
//	glPixelStorei(GL_PACK_ALIGNMENT,1);
//	glRasterPos2i((GLint)x, (GLint)y);
//	glDrawPixels((GLsizei)width, (GLsizei)height, (GLenum)format, GL_UNSIGNED_BYTE, data);

	const GLint maxTexSize = _context->maxTextureSize;
	if(width > maxTexSize || height > maxTexSize)
		printf("Driver::createTexture() - texture size w:%d, h:%d exceed the limit %d\n", width, height, maxTexSize);


	// TODO: It's slow, any alternative?
	width = width > maxTexSize ? maxTexSize : width;
	height = height > maxTexSize ? maxTexSize : height;
	void* texture = createTexture(NULL, width, height, format, data, format);

	setViewport(x, y, width, height);
	ortho(0, width, 0, height, 1, -1);
	setViewMatrix(_mat44Identity);

	SamplerState state = {
		texture,
		SamplerState::MIN_MAG_LINEAR,
		SamplerState::Edge,
		SamplerState::Edge
	};
	setSamplerState(0, state);

	drawQuad(
		0,0, width,0, width,height, 0,height,
		1,
		0,0, 1,0, 1,1, 0,1,
		255,255,255,255
	);

	deleteTexture(texture);
}

}	// Render
