#include "pch.h"
#include "roRenderDriver.h"

#include "../base/roArray.h"
#include "../base/roCpuProfiler.h"
#include "../base/roLog.h"
#include "../base/roMemory.h"
#include "../base/roString.h"
#include "../base/roStringHash.h"
#include "../base/roTypeCast.h"
#include "../math/roMath.h"

#include "../platform/roPlatformHeaders.h"

#if roCOMPILER_VC
#	include <gl/gl.h>
#	include "gl/glext.h"
#	include "platform.win/extensionsfwd.h"
#elif roOS_iOS
#	define RG_GLES
#	import <OpenGLES/ES1/gl.h>
#	import <OpenGLES/ES1/glext.h>
#	import <OpenGLES/ES2/gl.h>
#	import <OpenGLES/ES2/glext.h>
#	define glDepthRange glDepthRangef
#	define glClearDepth glClearDepthf
#	define GL_MIN GL_MIN_EXT
#	define GL_MAX GL_MAX_EXT
#	define GL_CLAMP_TO_BORDER GL_CLAMP_TO_EDGE
#	define glGenVertexArrays glGenVertexArraysOES
#	define glBindVertexArray glBindVertexArrayOES
#endif

// OpenGL stuffs
// Instancing:				http://sol.gfxile.net/instancing.html
// Pixel buffer object:		http://www.songho.ca/opengl/gl_pbo.html
// Silhouette shader:		http://prideout.net/blog/p54/Silhouette.glsl
// Buffer object streaming:	http://www.opengl.org/wiki/Buffer_Object_Streaming
// Geometry shader examples:http://renderingwonders.wordpress.com/tag/geometry-shader/
// Opengl on Mac:			http://developer.apple.com/library/mac/#documentation/GraphicsImaging/Conceptual/OpenGL-MacProgGuide/opengl_intro/opengl_intro.html

using namespace ro;

static const bool _usePbo = true;
static DefaultAllocator _allocator;

// ----------------------------------------------------------------------
// Common stuffs

//#define RG_GLES 1

#if roDEBUG
#	define checkError() { GLenum err = glGetError(); if(err != GL_NO_ERROR) roLog("error", "unhandled GL error 0x%04x before %s %d\n", err, __FILE__, __LINE__); }
#else
#	define checkError()
#endif

#define clearError() { glGetError(); }

static unsigned _hash(const void* data, roSize len)
{
	unsigned h = 0;
	const char* data_ = reinterpret_cast<const char*>(data);
	for(roSize i=0; i<len; ++i)
		h = data_[i] + (h << 6) + (h << 16) - h;
	return h;
}

// ----------------------------------------------------------------------
// Context management

// These functions are implemented in platform specific src files, eg. driver2.gl.windows.cpp
extern roRDriverContext* _newDriverContext_GL(roRDriver* driver);
extern void _deleteDriverContext_GL(roRDriverContext* self);
extern bool _initDriverContext_GL(roRDriverContext* self, void* platformSpecificWindow);
extern void _useDriverContext_GL(roRDriverContext* self);
extern roRDriverContext* _getCurrentContext_GL();

extern void _driverSwapBuffers_GL();
extern bool _driverChangeResolution_GL(unsigned width, unsigned height);

extern void rgDriverApplyDefaultState(roRDriverContext* self);

namespace {

#include "roRenderDriver.gl.inl"

static void _setViewport(unsigned x, unsigned y, unsigned width, unsigned height, float zmin, float zmax)
{
	glViewport((GLint)x, (GLint)y, (GLsizei)width, (GLsizei)height);
	glDepthRange(zmin, zmax);
}

static void _setScissorRect(unsigned x, unsigned y, unsigned width, unsigned height)
{
	// Get the current viewport first, to make y-axis pointing down
	GLint viewPort[4];
	glGetIntegerv(GL_VIEWPORT, viewPort);
	glScissor(x, viewPort[3] - height - y, width, height);
}

static void _clearColor(float r, float g, float b, float a)
{
	glClearColor(r, g, b, a);
	glClear(GL_COLOR_BUFFER_BIT);
}

static void _clearDepth(float z)
{
	// See: http://www.opengl.org/sdk/docs/man/xhtml/glClearDepth.xml
	glClearDepth(z);
	glClear(GL_DEPTH_BUFFER_BIT);
}

static void _clearStencil(unsigned char s)
{
	// See: http://www.opengl.org/sdk/docs/man/xhtml/glClearStencil.xml
	glClearStencil(s);
	glClear(GL_STENCIL_BUFFER_BIT);
}

static void _adjustDepthRangeMatrix(float* mat44)
{
	// Do nothing
	(void)mat44;
}

static bool _setRenderTargets(roRDriverTexture** textures, roSize targetCount, bool useDepthStencil)
{
	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_GL());
	if(!ctx) return false;

	if(!textures || targetCount == 0) {
		// Bind default frame buffer
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		return true;
	}

	// Make hash value
	unsigned hash = _hash(textures, sizeof(*textures) * targetCount);

	// Find render target cache
	for(unsigned i=0; i<ctx->renderTargetCache.size(); ++i) {
		if(ctx->renderTargetCache[i].hash == hash) {
			ctx->renderTargetCache[i].lastUsedTime = ctx->lastSwapTime;
			glBindFramebuffer(GL_FRAMEBUFFER, ctx->renderTargetCache[i].glh);
			checkError();
			return true;
		}
	}

	// Check the dimension of the render targets
	unsigned width = unsigned(-1);
	unsigned height = unsigned(-1);
	for(unsigned i=0; i<targetCount; ++i) {
		unsigned w = textures[i] ? textures[i]->width : ctx->width;
		unsigned h = textures[i] ? textures[i]->height : ctx->height;
		if(width == unsigned(-1)) width = w;
		if(height == unsigned(-1)) height = h;
		if(width != w || height != h) {
			roLog("error", "roRDriver setRenderTargets not all targets having the same dimension\n");
			return false;
		}
	}

	// Create depth and stencil as requested
	if(useDepthStencil) {
		// Search for existing depth and stencil buffers
	}

	// Create the FBO
	RenderTarget renderTarget = { hash, 0, ctx->lastSwapTime };
	glGenFramebuffers(1, &renderTarget.glh);
	glBindFramebuffer(GL_FRAMEBUFFER, renderTarget.glh);

	bool hasDepthAttachment = false;

	for(unsigned i=0; i<targetCount; ++i) {
		roRDriverTextureImpl* tex = static_cast<roRDriverTextureImpl*>(textures[i]);

		if(tex->format == roRDriverTextureFormat_DepthStencil) {
			if(hasDepthAttachment) {
				roAssert(false);
				roLog("warn", "roRDriver setRenderTargets detected multiple depth textures were specified, only the first one will be used\n");
			}
			else {
#if !defined(RG_GLES)
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, tex->glh, 0);
#else
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, tex->glh, 0);
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, tex->glh, 0);
#endif
			}

			checkError();
			hasDepthAttachment = true;
		}
		else
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, tex->glh, 0);
	}

	ctx->renderTargetCache.pushBack(renderTarget);

#if roDEBUG
	// See http://www.khronos.org/opengles/sdk/docs/man/xhtml/glCheckFramebufferStatus.xml
	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	roAssert(status != GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT);
	roAssert(status != GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT);
	roAssert(status != GL_FRAMEBUFFER_UNSUPPORTED);
	roAssert(status == GL_FRAMEBUFFER_COMPLETE);
	roAssert(GL_NO_ERROR == glGetError());
#endif

	checkError();

	return true;
}

// ----------------------------------------------------------------------
// State management

static const StaticArray<GLenum, 6> _blendOp = {
	GLenum(-1),
	GL_FUNC_ADD,
	GL_FUNC_SUBTRACT,
	GL_FUNC_REVERSE_SUBTRACT,
	GL_MIN,
	GL_MAX,
};

static const StaticArray<GLenum, 11> _blendValue = {
	GLenum(-1),
	GL_ZERO,
	GL_ONE,
	GL_SRC_COLOR,
	GL_ONE_MINUS_SRC_COLOR,
	GL_SRC_ALPHA,
	GL_ONE_MINUS_SRC_ALPHA,
	GL_DST_COLOR,
	GL_ONE_MINUS_DST_COLOR,
	GL_DST_ALPHA,
	GL_ONE_MINUS_DST_ALPHA,
};

// See: http://www.opengl.org/wiki/Blending
static void _setBlendState(roRDriverBlendState* state)
{
	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_GL());
	if(!state || !ctx) return;

	// Generate the hash value if not yet
	if(state->hash == 0)
		state->hash = (void*)_hash(state, sizeof(*state));

	if(state->hash == ctx->currentBlendStateHash)
		return;
	else {
		// TODO: Make use of the hash value, if OpenGL support state block
	}

	ctx->currentBlendStateHash = state->hash;

	if(state->wirteMask != ctx->currentColorWriteMask) {
		glColorMask(
			(state->wirteMask & roRDriverColorWriteMask_EnableRed) > 0,
			(state->wirteMask & roRDriverColorWriteMask_EnableGreen) > 0,
			(state->wirteMask & roRDriverColorWriteMask_EnableBlue) > 0,
			(state->wirteMask & roRDriverColorWriteMask_EnableAlpha) > 0
		);
		ctx->currentColorWriteMask = state->wirteMask;
	}

	if(state->enable)
		glEnable(GL_BLEND);
	else {
		glDisable(GL_BLEND);
		return;
	}

	// See: http://www.opengl.org/sdk/docs/man/xhtml/glBlendEquationSeparate.xml
	glBlendEquationSeparate(
		_blendOp[state->colorOp],
		_blendOp[state->alphaOp]
	);

/*	glBlendFunc(
		_blendValue[state->colorSrc],
		_blendValue[state->colorDst]
	);*/
	// See: http://www.opengl.org/sdk/docs/man/xhtml/glBlendFuncSeparate.xml
	glBlendFuncSeparate(
		_blendValue[state->colorSrc],
		_blendValue[state->colorDst],
		_blendValue[state->alphaSrc],
		_blendValue[state->alphaDst]
	);
}

static const StaticArray<GLenum, 4> _cullMode = {
	GLenum(-1),
	GLenum(-1),
	GL_FRONT,
	GL_BACK,
};

static void _setRasterizerState(roRDriverRasterizerState* state)
{
	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_GL());
	if(!state || !ctx) return;

	// Generate the hash value if not yet
	if(state->hash == 0)
		state->hash = (void*)_hash(state, sizeof(*state));

	if(state->hash == ctx->currentRasterizerStateHash)
		return;
	else {
		// TODO: Make use of the hash value, if OpenGL support state block
	}

	ctx->currentRasterizerStateHash = state->hash;

	if(state->scissorEnable)
		glEnable(GL_SCISSOR_TEST);
	else
		glDisable(GL_SCISSOR_TEST);

	if(state->smoothLineEnable)
		glEnable(GL_LINE_SMOOTH);
	else
		glDisable(GL_LINE_SMOOTH);

	if(state->multisampleEnable)
		glEnable(GL_MULTISAMPLE);
	else
		glDisable(GL_MULTISAMPLE);

	if(state->cullMode == roRDriverCullMode_None)
		glDisable(GL_CULL_FACE);
	else {
		glEnable(GL_CULL_FACE);
		glCullFace(_cullMode[state->cullMode]);
	}

	glFrontFace(state->isFrontFaceClockwise ? GL_CW : GL_CCW);
}

static const StaticArray<GLenum, 9> _compareFunc = {
	GLenum(-1),
	GL_NEVER,
	GL_LESS,
	GL_EQUAL,
	GL_LEQUAL,
	GL_GREATER,
	GL_NOTEQUAL,
	GL_GEQUAL,
	GL_ALWAYS,
};

static const StaticArray<GLenum, 9> _stencilOp = {
	GLenum(-1),
	GL_ZERO,
	GL_INVERT,
	GL_KEEP,
	GL_REPLACE,
	GL_INCR,
	GL_DECR,
	GL_INCR_WRAP,
	GL_DECR_WRAP,
};

static void _setDepthStencilState(roRDriverDepthStencilState* state)
{
	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_GL());
	if(!state || !ctx) return;

	// Generate the hash value if not yet
	if(state->hash == 0) {
		state->hash = (void*)_hash(
			&state->enableDepthTest,
			sizeof(roRDriverDepthStencilState) - roOffsetof(roRDriverDepthStencilState, roRDriverDepthStencilState::enableDepthTest)
		);
	}

	if(state->hash == ctx->currentDepthStencilStateHash)
		return;
	else {
		// TODO: Make use of the hash value, if OpenGL support state block
	}

	ctx->currentDepthStencilStateHash = state->hash;

	if(!state->enableDepthTest) {
		glDisable(GL_DEPTH_TEST);
	}
	else {
		glEnable(GL_DEPTH_TEST);
		if(ctx->currentDepthFunc != state->depthFunc) {
			ctx->currentDepthFunc = state->depthFunc;
			glDepthFunc(_compareFunc[state->depthFunc]);
		}
	}

	glDepthMask(state->enableDepthWrite);

	if(!state->enableStencil) {
		glDisable(GL_STENCIL_TEST);
	}
	else {
		glEnable(GL_STENCIL_TEST);
		glStencilFuncSeparate(
			GL_FRONT,
			_compareFunc[state->front.func],
			state->stencilRefValue,
			state->stencilMask
		);
		glStencilFuncSeparate(
			GL_BACK,
			_compareFunc[state->back.func],
			state->stencilRefValue,
			state->stencilMask
		);

		// See: http://www.opengl.org/sdk/docs/man/xhtml/glStencilOpSeparate.xml
		glStencilOpSeparate(
			GL_FRONT,
			_stencilOp[state->front.failOp],
			_stencilOp[state->front.zFailOp],
			_stencilOp[state->front.passOp]
		);

		glStencilOpSeparate(
			GL_BACK,
			_stencilOp[state->back.failOp],
			_stencilOp[state->back.zFailOp],
			_stencilOp[state->back.passOp]
		);
	}
}

static const StaticArray<GLenum, 5> _magFilter = { GLenum(-1), GL_NEAREST, GL_LINEAR, GL_NEAREST, GL_LINEAR };
static const StaticArray<GLenum, 5> _minFilter = { GLenum(-1), GL_NEAREST, GL_LINEAR, GL_NEAREST_MIPMAP_NEAREST, GL_LINEAR_MIPMAP_LINEAR };
static const StaticArray<GLenum, 5> _textureAddressMode = { GLenum(-1), GL_REPEAT, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_BORDER, GL_MIRRORED_REPEAT };

// For OpenGl sampler object, see: http://www.geeks3d.com/20110908/opengl-3-3-sampler-objects-control-your-texture-units/
// and http://www.opengl.org/registry/specs/ARB/sampler_objects.txt
static void _setTextureState(roRDriverTextureState* states, roSize stateCount, unsigned startingTextureUnit)
{
	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_GL());
	if(!ctx || !states || stateCount == 0) return;

#if !defined(RG_GLES)
	for(unsigned i=0; i<stateCount; ++i)
	{
		roRDriverTextureState& state = states[i];

		// Generate the hash value if not yet
		if(state.hash == 0) {
			state.hash = (void*)_hash(
				&state.filter,
				sizeof(roRDriverTextureState) - roOffsetof(roRDriverTextureState, roRDriverTextureState::filter)
			);
		}

		GLuint glh = 0;
		roSize freeCacheSlot = roSize(-1);

		// Try to search for the state in the cache
		for(roSize j=0; j<ctx->textureStateCache.size(); ++j) {
			if(state.hash == ctx->textureStateCache[j].hash) {
				glh = ctx->textureStateCache[j].glh;
				break;
			}
			if(freeCacheSlot == -1 && ctx->textureStateCache[j].glh == 0)
				freeCacheSlot = j;
		}

		// Cache miss, create the state object
		if(glh == 0) {
			// Not enough cache slot
			if(freeCacheSlot == -1) {
				glBindSampler(startingTextureUnit + i, 0);
				roLog("error", "roRDriver texture cache slot full\n");
				return;
			}

			glGenSamplers(1, &glh);
			glSamplerParameteri(glh, GL_TEXTURE_WRAP_S, _textureAddressMode[state.u]);
			glSamplerParameteri(glh, GL_TEXTURE_WRAP_T, _textureAddressMode[state.v]);
			glSamplerParameteri(glh, GL_TEXTURE_MAG_FILTER, _magFilter[state.filter]);
			glSamplerParameteri(glh, GL_TEXTURE_MIN_FILTER, _minFilter[state.filter]);
			state.maxAnisotropy = (unsigned)roClamp((float)state.maxAnisotropy, ctx->glCapability.minAnisotropic, ctx->glCapability.maxAnisotropic);
			glSamplerParameterf(glh, GL_TEXTURE_MAX_ANISOTROPY_EXT, (float)state.maxAnisotropy);

			ctx->textureStateCache[freeCacheSlot].glh = glh;
			ctx->textureStateCache[freeCacheSlot].hash = state.hash;
		}

		glBindSampler(startingTextureUnit + i, glh);
	}
#else
#endif
}

// ----------------------------------------------------------------------
// Buffer

// See: http://www.opengl.org/wiki/Buffer_Object

static const StaticArray<GLenum, 4> _bufferTarget = {
	0,
	GL_ARRAY_BUFFER,
	GL_ELEMENT_ARRAY_BUFFER,
#if !defined(RG_GLES)
	GL_UNIFORM_BUFFER
#endif
};

#if !defined(RG_GLES)
static const StaticArray<GLenum, 4> _bufferMapUsage = {
	0,
	GL_READ_ONLY,
	GL_WRITE_ONLY,
	GL_READ_WRITE,
};
static const StaticArray<GLenum, 4> _bufferRangeMapUsage = {
	0,
	GL_MAP_READ_BIT,
	GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT,
	GL_MAP_READ_BIT | GL_MAP_WRITE_BIT,
};
#endif

static roRDriverBuffer* _newBuffer()
{
	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_GL());

	if(!ctx->bufferCache.isEmpty()) {
		roRDriverBufferImpl* ret = ctx->bufferCache.back();
		ctx->bufferCache.popBack();
		return ret;
	}

	roRDriverBufferImpl* ret = _allocator.newObj<roRDriverBufferImpl>().unref();
	roZeroMemory(ret, sizeof(*ret));
	return ret;
}

static void _deleteBuffer(roRDriverBuffer* self)
{
	roRDriverBufferImpl* impl = static_cast<roRDriverBufferImpl*>(self);
	if(!impl) return;

	roAssert(!impl->isMapped);

	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_GL());

	// NOTE: We won't delete the buffer object right here, because it will unbind from VAO, where
	// we have no stored information to invalidate that vao.
	ctx->bufferCache.pushBack(impl);

	return;
}

static const StaticArray<GLenum, 4> _bufferUsage = {
	0,
	GL_STATIC_DRAW,
	GL_STREAM_DRAW,
	GL_DYNAMIC_DRAW
};

static bool _initBufferSpecificLocation(roRDriverBufferImpl* impl, roRDriverBufferType type, roRDriverDataUsage usage, const void* initData, roSize sizeInBytes, bool systemMemory)
{
	checkError();

	if(systemMemory) {
		impl->systemBuf = _allocator.realloc(impl->systemBuf, impl->sizeInBytes, sizeInBytes);
		if(initData)
 			roMemcpy(impl->systemBuf, initData, sizeInBytes);

		if(impl->glh) {
			impl->isMapped = false;	// Calling glDeleteBuffers also implies glUnmapBuffer
			glDeleteBuffers(1, &impl->glh);
			impl->glh = 0;
		}
 	}
 	else
	{
		if(!impl->glh)
			glGenBuffers(1, &impl->glh);

		GLenum t = _bufferTarget[type];
		roAssert("Invalid roRDriverBufferType" && t != 0);
		glBindBuffer(t, impl->glh);
		glBufferData(t, sizeInBytes, initData, _bufferUsage[usage]);
		roAssert(impl->glh);

		_allocator.free(impl->systemBuf);
		impl->systemBuf = NULL;
	}

	checkError();

	impl->type = type;
	impl->usage = usage;
	impl->isMapped = false;
	impl->mapOffset = 0;
	impl->mapSize = 0;
	impl->sizeInBytes = sizeInBytes;

	return true;
}

static bool _initBuffer(roRDriverBuffer* self, roRDriverBufferType type, roRDriverDataUsage usage, const void* initData, roSize sizeInBytes)
{
	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_GL());
	roRDriverBufferImpl* impl = static_cast<roRDriverBufferImpl*>(self);
	if(!ctx || !impl) return false;

	roAssert(!impl->isMapped);
	if(sizeInBytes == 0) initData = NULL;

	// For newer Opengl driver, we won't use any system memory. At least glVertexAttribPointer() always want a VBO
	bool keepInSystemMemory = (usage == roRDriverDataUsage_Stream && ctx->majorVersion <= 2);
	return _initBufferSpecificLocation(impl, type, usage, initData, sizeInBytes, keepInSystemMemory);
}

static bool _updateBuffer(roRDriverBuffer* self, roSize offsetInBytes, const void* data, roSize sizeInBytes)
{
	roRDriverBufferImpl* impl = static_cast<roRDriverBufferImpl*>(self);
	if(!impl) return false;
	if(impl->isMapped) return false;
	if(offsetInBytes != 0 && offsetInBytes + sizeInBytes > self->sizeInBytes) return false;
	if(impl->usage == roRDriverDataUsage_Static) return false;

	if(!data || sizeInBytes == 0) return true;

	if(impl->systemBuf) {
		if(sizeInBytes > self->sizeInBytes) {
			_allocator.realloc(impl->systemBuf, self->sizeInBytes, sizeInBytes);
			self->sizeInBytes = sizeInBytes;
		}
 		roMemcpy(((char*)impl->systemBuf) + offsetInBytes, data, sizeInBytes);
	}
 	else {
		checkError();
		GLenum t = _bufferTarget[self->type];
		glBindBuffer(t, impl->glh);
		if(sizeInBytes > self->sizeInBytes)
			glBufferData(t, sizeInBytes, data, _bufferUsage[impl->usage]);
		else
			glBufferSubData(t, offsetInBytes, sizeInBytes, data);
		checkError();
	}

	return true;
}

static void _unmapBuffer(roRDriverBuffer* self);

// Reference: http://www.opengl.org/wiki/Buffer_Object#Mapping
static void* _mapBuffer(roRDriverBuffer* self, roRDriverMapUsage usage, roSize offsetInBytes, roSize sizeInBytes)
{
	roRDriverBufferImpl* impl = static_cast<roRDriverBufferImpl*>(self);
	if(!impl) return NULL;

	if(impl->isMapped) return NULL;

	sizeInBytes = (sizeInBytes == 0) ? impl->sizeInBytes : sizeInBytes;
	if(offsetInBytes + sizeInBytes > impl->sizeInBytes)
		return NULL;

	void* ret = NULL;

	if(impl->systemBuf) {
		ret = ((char*)impl->systemBuf) + offsetInBytes;
	}
	else
	{
		checkError();
		roAssert("Invalid roRDriverMapUsage" && _bufferMapUsage[usage] != 0);
		GLenum t = _bufferTarget[self->type];
		glBindBuffer(t, impl->glh);

		// The write discard optimization
		if(!(usage & roRDriverMapUsage_Read))
			glBufferData(t, impl->sizeInBytes, NULL, GL_STREAM_DRAW);

#if !defined(RG_GLES)
		// glMapBufferRange only available for GL3 or above
		if(glMapBufferRange)
			ret = glMapBufferRange(t, offsetInBytes, sizeInBytes, _bufferRangeMapUsage[usage]);
		else
			ret = ((char*)glMapBuffer(t, _bufferMapUsage[usage])) + offsetInBytes;
#else
		// GLES didn't support read back of buffer
		if(usage & roRDriverMapUsage_Read)
		{
			roAssert(false && "GLES didn't support read back of buffer");
			return NULL;
		}
#endif

		// NOTE: It's better to un-bind it, since we might call unmapBuffer()
		// at a much much later time than mapBuffer()
		glBindBuffer(t, 0);
		checkError();
	}

	impl->isMapped = true;
	impl->mapUsage = usage;
	impl->mapOffset = offsetInBytes;
	impl->mapSize = sizeInBytes;

	if(!ret)
		_unmapBuffer(self);

	return ret;
}

static void _unmapBuffer(roRDriverBuffer* self)
{
	roRDriverBufferImpl* impl = static_cast<roRDriverBufferImpl*>(self);
	if(!impl || !impl->isMapped) return;

	if(impl->systemBuf) {
		// Nothing need to do to un-map the system memory
		impl->isMapped = false;
		return;
	}

#if !defined(RG_GLES)
	checkError();
	GLenum t = _bufferTarget[self->type];
	glBindBuffer(t, impl->glh);
	glUnmapBuffer(t);
	glBindBuffer(t, 0);
	checkError();
#endif

	impl->isMapped = false;
	impl->mapSize = 0;
}

static bool _resizeBuffer(roRDriverBuffer* self, roSize sizeInBytes)
{
	roRDriverBufferImpl* impl = static_cast<roRDriverBufferImpl*>(self);
	if(!impl) return false;
	if(impl->isMapped) return false;
	if(impl->usage == roRDriverDataUsage_Static) return false;

	if(impl->systemBuf) {
		_allocator.realloc(impl->systemBuf, self->sizeInBytes, sizeInBytes);
		self->sizeInBytes = sizeInBytes;
	}
	else {
		// TODO: This implementation will change the value of impl->glh!
		// which gives problem when roRDriverBuffer is binded to the shader buffers,
		// we should try our best to keep impl->glh unchange!
		roRDriverBuffer* newBuf = _newBuffer();
		if(!_initBuffer(newBuf, impl->type, impl->usage, NULL, sizeInBytes)) return false;

		if(void* mapped = _mapBuffer(self, roRDriverMapUsage_Read, 0, impl->sizeInBytes)) {
			// TODO: May consider ARB_copy_buffer?
			if(!_updateBuffer(newBuf, 0, mapped, impl->sizeInBytes))
				return false;
			_unmapBuffer(self);

			roSwapMemory(impl, newBuf, sizeof(*impl));
			_deleteBuffer(newBuf);
			return true;
		}

		return false;
	}

	return true;
}

static bool _switchBufferMode(roRDriverBufferImpl* impl)
{
	roAssert((impl->glh != 0) != (impl->systemBuf != NULL) && "Only glh or system buffer but not using both");

	if(impl->glh) {
		if(void* data = _mapBuffer(impl, roRDriverMapUsage_Read, 0, 0))
			return _initBufferSpecificLocation(impl, impl->type, impl->usage, data, impl->sizeInBytes, true);
	}
	else if(impl->systemBuf) {
		return _initBufferSpecificLocation(impl, impl->type, impl->usage, impl->systemBuf, impl->sizeInBytes, false);
	}
	else
		roAssert(false);

	return false;
}

// ----------------------------------------------------------------------
// Texture

TextureFormatMapping _textureFormatMappings[] = {
	{ roRDriverTextureFormat(0),			0, 0, 0, 0 },
#if !defined(RG_GLES)
	{ roRDriverTextureFormat_RGBA,			4,	GL_RGBA8, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV },	// NOTE: Endianness
	{ roRDriverTextureFormat_RGBA_16F,		8,	GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT },
	{ roRDriverTextureFormat_RGBA_32F,		16,	GL_RGBA32F, GL_RGBA, GL_FLOAT },
	{ roRDriverTextureFormat_RGB_16F,		6,	GL_RGB16F, GL_RGB, GL_FLOAT },
	{ roRDriverTextureFormat_RGB_32F,		12,	GL_RGB32F, GL_RGB, GL_FLOAT },
	{ roRDriverTextureFormat_L,				1,	GL_LUMINANCE8, GL_LUMINANCE, GL_UNSIGNED_BYTE },
	{ roRDriverTextureFormat_A,				1,	GL_ALPHA, GL_ALPHA, GL_UNSIGNED_BYTE },
	{ roRDriverTextureFormat_Depth,			0,	0, 0, 0 },
	{ roRDriverTextureFormat_DepthStencil,	4,	GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8 },
	{ roRDriverTextureFormat_PVRTC2,		0,	0, 0, 0 },
	{ roRDriverTextureFormat_PVRTC4,		0,	0, 0, 0 },
//	{ roRDriverTextureFormat_PVRTC2,		2,	GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG, 0, 0 },
//	{ roRDriverTextureFormat_PVRTC4,		1,	GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG, 0, 0 },
	{ roRDriverTextureFormat_DXT1,			0,	0, 0, 0 },
	{ roRDriverTextureFormat_DXT5,			0,	0, 0, 0 },
#else
	{ roRDriverTextureFormat_RGBA,			4,	GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE },
	{ roRDriverTextureFormat_L,				1,	GL_LUMINANCE, GL_LUMINANCE, GL_UNSIGNED_BYTE },
	{ roRDriverTextureFormat_A,				1,	GL_ALPHA, GL_ALPHA, GL_UNSIGNED_BYTE },
	{ roRDriverTextureFormat_Depth,			0,	0, 0, 0 },
	{ roRDriverTextureFormat_DepthStencil,	4,	GL_DEPTH24_STENCIL8_OES, GL_DEPTH_STENCIL_OES, GL_UNSIGNED_INT_24_8_OES },
	{ roRDriverTextureFormat_PVRTC2,		0,	0, 0, 0 },
	{ roRDriverTextureFormat_PVRTC4,		0,	0, 0, 0 },
//	{ roRDriverTextureFormat_PVRTC2,		2,	GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG, 0, 0 },
//	{ roRDriverTextureFormat_PVRTC4,		1,	GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG, 0, 0 },
	{ roRDriverTextureFormat_DXT1,			0,	0, 0, 0 },
	{ roRDriverTextureFormat_DXT5,			0,	0, 0, 0 },
#endif
};

static roRDriverTexture* _newTexture()
{
	roRDriverTextureImpl* ret = _allocator.newObj<roRDriverTextureImpl>().unref();
	roZeroMemory(ret, sizeof(*ret));
	ret->isYAxisUp = true;
	return ret;
}

static void _deleteTexture(roRDriverTexture* self)
{
	roRDriverTextureImpl* impl = static_cast<roRDriverTextureImpl*>(self);
	if(!impl) return;

	for(roSize i=0; i<impl->mapInfo.size(); ++i) {
		roAssert(!impl->mapInfo[i].glPbo && "Call unmapTexture() for all mip-map and tex array befor calling deleteTexture");
		roAssert(!impl->mapInfo[i].systemBuf && "Call unmapTexture() for all mip-map and tex array befor calling deleteTexture");
	}

	if(impl->glh)
		glDeleteTextures(1, &impl->glh);

	_allocator.deleteObj(static_cast<roRDriverTextureImpl*>(self));
}

static bool _initTexture(roRDriverTexture* self, unsigned width, unsigned height, unsigned maxMipLevels, roRDriverTextureFormat format, roRDriverTextureFlag flags)
{
	roRDriverTextureImpl* impl = static_cast<roRDriverTextureImpl*>(self);
	if(!impl) return false;

	impl->width = width;
	impl->height = height;
	impl->maxMipLevels = maxMipLevels;
	impl->format = format;
	impl->flags = flags;
	impl->glTarget = GL_TEXTURE_2D;
	impl->formatMapping = &(_textureFormatMappings[format]);

	for(roSize i=0; i<impl->mapInfo.size(); ++i) {
		roAssert(!impl->mapInfo[i].glPbo && "Call unmapTexture() for all mip-map and tex array befor calling initTexture");
		roAssert(!impl->mapInfo[i].systemBuf && "Call unmapTexture() for all mip-map and tex array befor calling initTexture");
	}

	if(!impl->glh)
		glGenTextures(1, &impl->glh);

	glBindTexture(impl->glTarget, impl->glh);

	TextureFormatMapping* mapping = impl->formatMapping;
	glTexImage2D(
		impl->glTarget, 0, mapping->glInternalFormat,
		width, height, 0,
		mapping->glFormat, mapping->glType,
		NULL
	);

	return true;
}

static unsigned _mipLevelInfo(roRDriverTextureImpl* self, unsigned mipIndex, unsigned& mipSize)
{
	mipSize = 0;
	unsigned offset = 0;

	unsigned mipWidth = self->width;
	unsigned mipHeight = self->height;

	for(unsigned i=0; i<=mipIndex; ++i) {
		mipSize = mipWidth * mipHeight;

		if(roRDriverTextureFormat_Compressed & self->format)
			mipSize = mipSize >> self->formatMapping->glPixelSize;
		else
			mipSize = mipSize * self->formatMapping->glPixelSize;

		if(i > 0) {
			offset += mipSize;
			if(mipWidth > 1) mipWidth /= 2;
			if(mipHeight > 1) mipHeight /= 2;
		}
	}

	return offset;
}

static GLuint _getPbo(roRDriverContextImpl* ctx)
{
	GLuint pbo = 0;
	for(roSize i=0; i<ctx->pixelBufferCache.size(); ++i) {
		roSize index = ctx->currentPixelBufferIndex++;
		ctx->currentPixelBufferIndex = ctx->currentPixelBufferIndex % ctx->pixelBufferCache.size();
		pbo = ctx->pixelBufferCache[index];

		if(!ctx->pixelBufferInUse.find(pbo))
			return pbo;
		else
			pbo = pbo;
	}

	glGenBuffers(1, &pbo);
	ctx->pixelBufferCache.pushBack(pbo);
	return pbo;
}

static bool _updateTexture(roRDriverTexture* self, unsigned mipIndex, unsigned aryIndex, const void* data, roSize rowPaddingInBytes, roSize* bytesRead)
{
	if(bytesRead) *bytesRead = 0;

	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_GL());
	roRDriverTextureImpl* impl = static_cast<roRDriverTextureImpl*>(self);
	if(!ctx || !impl || !impl->glh) return false;
	if(!impl->format) return false;

	checkError();

	unsigned mipSize = 0;
	unsigned mipw = roMaxOf2<unsigned>(1, impl->width >> mipIndex);
	unsigned miph = roMaxOf2<unsigned>(1, impl->height >> mipIndex);
	_mipLevelInfo(impl, mipIndex, mipSize);
	if(bytesRead) *bytesRead = mipSize;

#if !defined(RG_GLES)
	// Using PBO
	if(_usePbo) {
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, _getPbo(ctx));
		glBufferData(GL_PIXEL_UNPACK_BUFFER, mipSize, data, GL_STREAM_DRAW);
		data = NULL;	// If we are using PBO, no need to supply the data pointer to glTexImage2D()
	}
#endif

	glBindTexture(impl->glTarget, impl->glh);

	// Give the texture object a valid sampler state, even we might use sampler object
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	TextureFormatMapping* mapping = impl->formatMapping;

	if(impl->format & roRDriverTextureFormat_Compressed) {
		unsigned imgSize = (mipw * miph) >> mapping->glPixelSize;
		glCompressedTexImage2D(
			impl->glTarget, mipIndex, mapping->glInternalFormat,
			mipw, miph, 0,
			imgSize,
			data
		);
	}
	else {
		GLint alignmentBackup;
		glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignmentBackup);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

		// Handling the empty padding at the end of each row of pixels
		// NOTE: GL_UNPACK_ROW_LENGTH might help, but OpenGL ES didn't support it
		// See: http://stackoverflow.com/questions/205522/opengl-subtexturing
		if(rowPaddingInBytes == 0) {
			glTexImage2D(
				impl->glTarget, mipIndex, mapping->glInternalFormat,
				mipw, miph, 0,
				mapping->glFormat, mapping->glType,
				data
			);
		}
		else {
			// Create an empty texture object first
			glTexImage2D(
				impl->glTarget, mipIndex, mapping->glInternalFormat,
				mipw, miph, 0,
				mapping->glFormat, mapping->glType,
				NULL
			);

			// Then fill the pixels row by row
			for(unsigned y=0; y<miph; ++y) {
				const unsigned char* row = ((const unsigned char*)data) + y * (mipw * mapping->glPixelSize + rowPaddingInBytes);
				glTexSubImage2D(
					impl->glTarget, mipIndex, 0, y,
					mipw, 1,
					mapping->glFormat, mapping->glType,
					row
				);
			}

			if(bytesRead)
				*bytesRead += rowPaddingInBytes * miph;
		}

		glPixelStorei(GL_UNPACK_ALIGNMENT, alignmentBackup);
	}

#if !defined(RG_GLES)
	if(_usePbo)
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
#endif

	checkError();

	return true;
}

static void _unmapTexture(roRDriverTexture* self, unsigned mipIndex, unsigned aryIndex);

// Reference: http://www.seas.upenn.edu/~pcozzi/OpenGLInsights/OpenGLInsights-AsynchronousBufferTransfers.pdf
static void* _mapTexture(roRDriverTexture* self, roRDriverMapUsage usage, unsigned mipIndex, unsigned aryIndex, roSize& rowBytes)
{
	CpuProfilerScope cpuProfilerScope(__FUNCTION__);

	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_GL());
	roRDriverTextureImpl* impl = static_cast<roRDriverTextureImpl*>(self);
	if(!ctx || !impl) return NULL;
	if(!impl->format) return NULL;

	bool isCubeMap = false;
	unsigned bufferIndex = mipIndex * (isCubeMap ? 6 : 1) + aryIndex;

	if(bufferIndex >= impl->mapInfo.size()) {
		roRDriverTextureImpl::MapInfo mapInfo = { 0 };
		impl->mapInfo.resize(bufferIndex + 1, mapInfo);
	}

	TextureFormatMapping* mapping = impl->formatMapping;
	unsigned mipw = roMaxOf2<unsigned>(1, impl->width >> mipIndex);
	unsigned miph = roMaxOf2<unsigned>(1, impl->height >> mipIndex);
	roSize sizeInBytes = mapping->glPixelSize * mipw * miph;

	checkError();

	void* ret = NULL;
	roRDriverTextureImpl::MapInfo& mapInfo = impl->mapInfo[bufferIndex];
	mapInfo.usage = usage;

#if !defined(RG_GLES)
	if(_usePbo && !mapInfo.glPbo)
	{
		mapInfo.glPbo = _getPbo(ctx);

		if(usage & roRDriverMapUsage_Read) {
			glBindBuffer(GL_PIXEL_PACK_BUFFER, mapInfo.glPbo);
			glBufferData(GL_PIXEL_PACK_BUFFER, sizeInBytes, NULL, GL_STREAM_READ);
			glBindTexture(impl->glTarget, impl->glh);

			if(impl->format & roRDriverTextureFormat_Compressed) {
				glGetCompressedTexImage(
					impl->glTarget,
					mipIndex,
					0	// Offset to the texture
				);
			}
			else {
				glGetTexImage(
					impl->glTarget,
					mipIndex,
					mapping->glFormat, mapping->glType,
					0	// Offset to the texture
				);
			}

			if(ctx->driver->stallCallback && glFenceSync) {
				GLsync syn = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
				(*ctx->driver->stallCallback)(ctx->driver->stallCallbackUserData);
				GLint status = 0;
				GLsizei numReceived;

				// Wait until the sync object signaled
				while(glGetSynciv(syn, GL_SYNC_STATUS, 1, &numReceived, &status), status != GL_SIGNALED) {
					(*ctx->driver->stallCallback)(ctx->driver->stallCallbackUserData);
				}
				glDeleteSync(syn);
			}

			ret = glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, sizeInBytes, _bufferRangeMapUsage[usage]);
			glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
			glBindTexture(impl->glTarget, 0);
		}
		// NOTE: For roRDriverMapUsage_ReadWrite I assume there is no need to call glMapBufferRange with GL_PIXEL_UNPACK_BUFFER
		// for a write pointer, it should return the same pointer as the read pointer
		else if(usage & roRDriverMapUsage_Write) {	// This block of code is for write only situation
			glBindBuffer(GL_PIXEL_UNPACK_BUFFER, mapInfo.glPbo);
			glBufferData(GL_PIXEL_UNPACK_BUFFER, sizeInBytes, NULL, GL_DYNAMIC_DRAW);

			ret = glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, sizeInBytes, _bufferRangeMapUsage[usage]);
			glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
		}
		else {
			roAssert(false);
		}

		ctx->pixelBufferInUse.pushBack(mapInfo.glPbo);
	}
	else
#endif
	if(!mapInfo.systemBuf) {
		roSize sizeInBytes = impl->formatMapping->glPixelSize * impl->width * impl->height;
		mapInfo.systemBuf = _allocator.malloc(sizeInBytes);
		ret = mapInfo.systemBuf;

		if(usage & roRDriverMapUsage_Read) {
			glBindTexture(impl->glTarget, impl->glh);

			// TODO: This is a blocking operation, try to invoke the stallCallback() if possible
			glGetTexImage(
				impl->glTarget,
				mipIndex,
				mapping->glFormat, mapping->glType,
				ret
			);

			glBindTexture(impl->glTarget, 0);
		}
	}
	else {
		roAssert(false);
	}

	mapInfo.mappedPtr = (roByte*)ret;

	if(ret)
		rowBytes = impl->width * mapping->glPixelSize;
	else
		_unmapTexture(self, mipIndex, aryIndex);

	checkError();
	return ret;
}

static void _unmapTexture(roRDriverTexture* self, unsigned mipIndex, unsigned aryIndex)
{
	CpuProfilerScope cpuProfilerScope(__FUNCTION__);

	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_GL());
	roRDriverTextureImpl* impl = static_cast<roRDriverTextureImpl*>(self);
	if(!ctx || !impl) return;
	if(!impl->format) return;

	bool isCubeMap = false;
	unsigned bufferIndex = mipIndex * (isCubeMap ? 6 : 1) + aryIndex;

	roRDriverTextureImpl::MapInfo& mapInfo = impl->mapInfo[bufferIndex];

#if !defined(RG_GLES)
	if(_usePbo && mapInfo.glPbo)
	{
		checkError();

		if(mapInfo.usage & roRDriverMapUsage_Write)
		{
			// Only do the texture processing if the mapping was success
			if(mapInfo.mappedPtr)
			{
				glBindBuffer(GL_PIXEL_UNPACK_BUFFER, mapInfo.glPbo);
				glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);

				glBindTexture(impl->glTarget, impl->glh);

				TextureFormatMapping* mapping = impl->formatMapping;
				unsigned mipw = roMaxOf2<unsigned>(1, impl->width >> mipIndex);
				unsigned miph = roMaxOf2<unsigned>(1, impl->height >> mipIndex);

				// Copy the pbo to the texture
				if(impl->format & roRDriverTextureFormat_Compressed) {
					unsigned sizeInBytes = (mipw * miph) >> mapping->glPixelSize;
					glCompressedTexImage2D(
						impl->glTarget, mipIndex, mapping->glInternalFormat,
						mipw, miph, 0,
						sizeInBytes,
						0		// Offset into pbo
					);
				}
				else {
					GLint alignmentBackup;
					glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignmentBackup);
					glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

					glTexSubImage2D(
						impl->glTarget, mipIndex, 0, 0,
						mipw, miph,
						mapping->glFormat, mapping->glType,
						0		// Offset into pbo
					);

					glPixelStorei(GL_UNPACK_ALIGNMENT, alignmentBackup);
					checkError();
				}

				glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
				glBindTexture(impl->glTarget, 0);
			}
		}

		ctx->pixelBufferInUse.removeByKey(mapInfo.glPbo);
		mapInfo.glPbo = 0;
		mapInfo.mappedPtr = NULL;
	}
	else
#endif
	if(mapInfo.systemBuf) {
		_updateTexture(self, mipIndex, aryIndex, mapInfo.systemBuf, 0, NULL);
		_allocator.free(mapInfo.systemBuf);
		mapInfo.systemBuf = NULL;
	}
	else {
		roAssert(false);
	}
}

// ----------------------------------------------------------------------
// Shader

static const StaticArray<GLenum, 5> _shaderTypes = {
	GL_VERTEX_SHADER,
	GL_FRAGMENT_SHADER,
#if !defined(RG_GLES)
	GL_GEOMETRY_SHADER,
	GL_TESS_CONTROL_SHADER,
	GL_TESS_EVALUATION_SHADER,
#endif
};

static roRDriverShader* _newShader()
{
	roRDriverShaderImpl* ret = _allocator.newObj<roRDriverShaderImpl>().unref();
	roZeroMemory(ret, sizeof(*ret));
	return ret;
}

static void _deleteShader(roRDriverShader* self)
{
	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_GL());
	roRDriverShaderImpl* impl = static_cast<roRDriverShaderImpl*>(self);
	if(!ctx || !impl) return;

	// Remove any shader program that use this shader
	for(unsigned i=0; i<ctx->shaderProgramCache.size();) {
		roRDriverShaderProgramImpl& program = ctx->shaderProgramCache[i];

		roRDriverShaderImpl** cache = program.shaders.find(impl);

		// Destroy the shader program
		if(cache) {
			if(ctx->currentShaderProgram == &program) {
				ctx->currentShaderProgram = NULL;
				glUseProgram(0);
			}

			glDeleteProgram(program.glh);

			program.glh = 0;	// For safety
			program.hash = 0;	//

			ctx->shaderProgramCache.remove(i);
		}
		else
			++i;
	}

	if(impl->glh)
		glDeleteShader(impl->glh);

	_allocator.deleteObj(static_cast<roRDriverShaderImpl*>(self));
}

static bool _initShader(roRDriverShader* self, roRDriverShaderType type, const char** sources, roSize sourceCount, roByte** outBlob, roSize* outBlobSize)
{
	roRDriverShaderImpl* impl = static_cast<roRDriverShaderImpl*>(self);
	if(!impl || sourceCount == 0) return false;

	checkError();

	// Disallow re-init
	if(impl->glh)
		return false;

	self->type = type;
	impl->glh = glCreateShader(_shaderTypes[type]);

	glShaderSource(impl->glh, num_cast<GLsizei>(sourceCount), sources, NULL);
	glCompileShader(impl->glh);

	// Check compilation status
	GLint compileStatus;
	glGetShaderiv(impl->glh, GL_COMPILE_STATUS, &compileStatus);

	if(compileStatus == GL_FALSE) {
		GLint len;
		glGetShaderiv(impl->glh, GL_INFO_LOG_LENGTH, &len);
		if(len > 0) {
			String buf;
			buf.resize(len);
			glGetShaderInfoLog(impl->glh, len, NULL, buf.c_str());
			roLog("error", "glCompileShader failed: %s\n", buf.c_str());
		}

		clearError();
		return false;
	}

	checkError();

	return true;
}

bool _initShaderFromBlob(roRDriverShader* self, roRDriverShaderType type, const roByte* blob, roSize blobSize)
{
	return false;
}

static void _deleteShaderBlob(roByte* blob)
{
	_allocator.free(blob);
}

static ProgramUniform* _findProgramUniform(roRDriverContextImpl* ctx, roRDriverShaderProgramImpl* impl, unsigned nameHash)
{
	roAssert(ctx);
	if(!impl) return NULL;

	for(unsigned i=0; i<impl->uniforms.size(); ++i) {
		if(impl->uniforms[i].nameHash == nameHash)
			return &impl->uniforms[i];
	}
	return NULL;
}

static ProgramUniformBlock* _findProgramUniformBlock(unsigned nameHash)
{
	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_GL());
	if(!ctx) return NULL;

	roRDriverShaderProgramImpl* impl = ctx->currentShaderProgram;
	if(!impl) return NULL;

	for(unsigned i=0; i<impl->uniformBlocks.size(); ++i) {
		if(impl->uniformBlocks[i].nameHash == nameHash)
			return &impl->uniformBlocks[i];
	}
	return NULL;
}

static ProgramAttribute* _findProgramAttribute(roRDriverShaderProgram* self, unsigned nameHash)
{
	roRDriverShaderProgramImpl* impl = static_cast<roRDriverShaderProgramImpl*>(self);
	if(!impl) return NULL;

	for(unsigned i=0; i<impl->attributes.size(); ++i) {
		if(impl->attributes[i].nameHash == nameHash)
			return &impl->attributes[i];
	}
	return NULL;
}

static bool _initShaderProgram(roRDriverShaderProgram* self, roRDriverShader** shaders, roSize shaderCount)
{
	roRDriverShaderProgramImpl* impl = static_cast<roRDriverShaderProgramImpl*>(self);
	if(!impl || !shaders || shaderCount == 0) return false;

	checkError();

	impl->glh = glCreateProgram();

	impl->textureCount = 0;

	// Assign the hash value
	impl->hash = _hash(shaders, shaderCount * sizeof(*shaders));

	// Attach shaders
	for(unsigned i=0; i<shaderCount; ++i) {
		if(shaders[i]) {
			glAttachShader(impl->glh, static_cast<roRDriverShaderImpl*>(shaders[i])->glh);
			impl->shaders.pushBack(static_cast<roRDriverShaderImpl*>(shaders[i]));
		}
	}

	// Perform linking
	glLinkProgram(impl->glh);

	// Check link status
	GLint linkStatus;
	glGetProgramiv(impl->glh, GL_LINK_STATUS, &linkStatus);

	if(linkStatus == GL_FALSE) {
		GLint len;
		glGetProgramiv(impl->glh, GL_INFO_LOG_LENGTH, &len);
		if(len > 0) {
			String buf;
			buf.resize(len);
			glGetProgramInfoLog(impl->glh, len, NULL, buf.c_str());
			roLog("error", "glLinkProgram failed: %s\n", buf.c_str());
		}

		clearError();
		return false;
	}

	glUseProgram(impl->glh);

	checkError();

	{	// Query shader uniforms
		GLint uniformCount;
		glGetProgramiv(impl->glh, GL_ACTIVE_UNIFORMS, &uniformCount);

		impl->uniforms.resize(uniformCount);

		GLuint texunit = 0;

		for(GLint i=0; i<uniformCount; ++i)
		{
			GLchar uniformName[64];
			GLsizei uniformNameLength;
			GLint uniformSize;
			GLenum uniformType;

			// See: http://www.opengl.org/sdk/docs/man/xhtml/glGetActiveUniform.xml
			glGetActiveUniform(impl->glh, i, sizeof(uniformName), &uniformNameLength, &uniformSize, &uniformType, uniformName);

			// For array uniform, some driver may return var_name[0]
			for(unsigned j=0; j<sizeof(uniformName); ++j)
				if(uniformName[j] == '[')
					uniformName[j] = '\0';

			ProgramUniform* uniform = &impl->uniforms[i];
			uniform->nameHash = stringHash(uniformName, 0);
			uniform->location = glGetUniformLocation(impl->glh, uniformName);
			uniform->arraySize = uniformSize;
			uniform->type = uniformType;

			switch(uniformType) {
				case GL_SAMPLER_2D:
				case GL_SAMPLER_CUBE:
#if !defined(RG_GLES)
				case GL_SAMPLER_1D:
				case GL_SAMPLER_3D:
				case GL_SAMPLER_1D_SHADOW:
				case GL_SAMPLER_2D_SHADOW:
#endif
					glUniform1i(uniform->location, texunit++);	// Create mapping between uniform location and texture unit
					impl->textureCount++;
					break;
				default:
					uniform->texunit = -1;
					break;
			}
		}
	}

	checkError();

	// Query shader uniform block
	if(glGetUniformIndices)
	{
		GLint blockCount;
		glGetProgramiv(impl->glh, GL_ACTIVE_UNIFORM_BLOCKS, &blockCount);

		impl->uniformBlocks.resize(blockCount);

		for(GLint i=0; i<blockCount; ++i)
		{
			GLchar blockName[64];
			GLsizei blockNameLength;
			glGetActiveUniformBlockName(impl->glh, i, sizeof(blockName), &blockNameLength, blockName);

			GLint blockSize;
			glGetActiveUniformBlockiv(impl->glh, i, GL_UNIFORM_BLOCK_DATA_SIZE, &blockSize);

			ProgramUniformBlock* block = &impl->uniformBlocks[i];
			block->nameHash = stringHash(blockName, 0);
			block->index = i;	// NOTE: Differ than the uniform location, using i as index is ok

			glGetActiveUniformBlockiv(impl->glh, i, GL_UNIFORM_BLOCK_DATA_SIZE, &block->blockSizeInBytes);
		}
	}

	checkError();

	{	// Query shader attributes
		GLint attributeCount;
		glGetProgramiv(impl->glh, GL_ACTIVE_ATTRIBUTES, &attributeCount);

		impl->attributes.resize(attributeCount);

		for(GLint i=0; i<attributeCount; ++i)
		{
			char attributeName[64];
			GLsizei attributeNameLength;
			GLint attributeSize;
			GLenum attributeType;

			// See: http://www.opengl.org/sdk/docs/man/xhtml/glGetActiveAttrib.xml
			glGetActiveAttrib(impl->glh, i, sizeof(attributeName), &attributeNameLength, &attributeSize, &attributeType, attributeName);

			// For array attribute, some driver may return var_name[0]
			for(unsigned j=0; j<sizeof(attributeName); ++j)
				if(attributeName[j] == '[')
					attributeName[j] = '\0';

			ProgramAttribute* attribute = &impl->attributes[i];
			attribute = &impl->attributes[i];
			attribute->nameHash = stringHash(attributeName, 0);
			attribute->location = glGetAttribLocation(impl->glh, attributeName);
			// NOTE: From the Opengl documentation, this suppose to be the size of the arrtibute array,
			// but I just have no idea on how to make attribute as array in the first place
			attribute->type = attributeType;
			attribute->arraySize = attributeSize;

			switch(attributeType) {
				case GL_FLOAT:			attribute->elementType = GL_FLOAT;	attribute->elementCount = 1; break;
				case GL_FLOAT_VEC2:		attribute->elementType = GL_FLOAT;	attribute->elementCount = 2; break;
				case GL_FLOAT_VEC3:		attribute->elementType = GL_FLOAT;	attribute->elementCount = 3; break;
				case GL_FLOAT_VEC4:		attribute->elementType = GL_FLOAT;	attribute->elementCount = 4; break;
				case GL_INT:			attribute->elementType = GL_INT;	attribute->elementCount = 1; break;
				case GL_INT_VEC2:		attribute->elementType = GL_INT;	attribute->elementCount = 2; break;
				case GL_INT_VEC3:		attribute->elementType = GL_INT;	attribute->elementCount = 3; break;
				case GL_INT_VEC4:		attribute->elementType = GL_INT;	attribute->elementCount = 4; break;
				default: roAssert(false && "Not supported");
			}
		}
	}

	checkError();

	return true;
}

bool _bindShaders(roRDriverShader** shaders, roSize shaderCount)
{
	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_GL());
	if(!ctx) return false;

	if(!shaders || shaderCount == 0) {
		glUseProgram(0);
		return true;
	}

	// Hash the address of the shaders to see if a shader program is already created or not
	unsigned hash = _hash(shaders, shaderCount * sizeof(*shaders));

	roRDriverShaderProgramImpl* program = NULL;
	// Linear search in the shader program cache
	// TODO: Find using hash table
	for(unsigned i=0; i<ctx->shaderProgramCache.size(); ++i) {
		if(ctx->shaderProgramCache[i].hash == hash) {
			program = &ctx->shaderProgramCache[i];
			break;
		}
	}

	checkError();

	if(!program) {
		ctx->shaderProgramCache.pushBack();
		program = &ctx->shaderProgramCache.back();
		_initShaderProgram(program, shaders, shaderCount);
	}

	glUseProgram(program->glh);
	ctx->currentShaderProgram = program;

	checkError();

	return true;
}

// See: http://www.opengl.org/wiki/Uniform_Buffer_Object
// See: http://arcsynthesis.org/gltut/Positioning/Tut07%20Shared%20Uniforms.html
bool _setUniformBuffer(roRDriverShaderProgramImpl* program, unsigned nameHash, roRDriverBuffer* buffer, roRDriverShaderBufferInput* input)
{
	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_GL());
	if(!ctx) return false;

	if(!program) return false;

	roRDriverBufferImpl* bufferImpl = static_cast<roRDriverBufferImpl*>(buffer);
	if(!bufferImpl) return false;

	if(bufferImpl->type != roRDriverBufferType_Uniform)
		return false;

	checkError();

	do {	// Breakable scope

#if !defined(RG_GLES)
	// Find the nameHash in the uniform block
	if(ProgramUniformBlock* block = _findProgramUniformBlock(nameHash)) {
		if(int(bufferImpl->sizeInBytes) < block->blockSizeInBytes)
			return false;

		// Switch to buffer object mode, required by gl uniform block
		if(bufferImpl->glh == 0)
			roVerify(_switchBufferMode(bufferImpl));

		glBindBufferBase(GL_UNIFORM_BUFFER, block->index, bufferImpl->glh);
		glUniformBlockBinding(program->glh, block->index, block->index);
		break;
	}
#endif

	// Switch to system memory mode
	if(!bufferImpl->systemBuf)
		roVerify(_switchBufferMode(bufferImpl));

	if(ProgramUniform* uniform = _findProgramUniform(ctx, program, nameHash)) {
		char* data = (char*)(bufferImpl->systemBuf) + input->offset;
		switch(uniform->type) {
		case GL_FLOAT_VEC2:
			glUniform2fv(uniform->location, uniform->arraySize, (float*)data);
			break;
		case GL_FLOAT_VEC3:
			glUniform3fv(uniform->location, uniform->arraySize, (float*)data);
			break;
		case GL_FLOAT_VEC4:
			glUniform4fv(uniform->location, uniform->arraySize, (float*)data);
			break;
		case GL_FLOAT_MAT4:
			glUniformMatrix4fv(uniform->location, uniform->arraySize, false, (float*)data);
			break;
		default: break;
		}
	}

	} while(false);	// Breakable scope

	checkError();

	return true;
}

bool _bindShaderTexture(roRDriverShaderTextureInput* inputs, roSize inputCount)
{
	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_GL());
	if(!ctx || !inputs) return false;

	roRDriverShaderProgramImpl* program = static_cast<roRDriverShaderProgramImpl*>(ctx->currentShaderProgram);
	if(!program) return false;

	checkError();

	for(roSize i=0; i<inputCount; ++i)
	{
		roRDriverShaderTextureInput& input = inputs[i];

		// Generate the hash value if not yet
		if(input.nameHash == 0)
			input.nameHash = stringHash(input.name, 0);

		ProgramUniform* uniform = _findProgramUniform(ctx, program, input.nameHash);
		if(!uniform) {
			roLog("error", "bindShaderTextures() can't find the shader param '%s'!\n", input.name ? input.name : "");
			continue;
		}

		glActiveTexture(GL_TEXTURE0 + uniform->texunit);

		roRDriverTextureImpl* impl = static_cast<roRDriverTextureImpl*>(input.texture);
		if(impl) {
			GLenum glTarget = impl->glTarget;
			GLuint glh = impl->glh;

			if(glTarget == 0)
				glTarget = GL_TEXTURE_2D;

			glBindTexture(glTarget, glh);
//			glTexParameteri(glTarget, GL_TEXTURE_MAG_FILTER, crGL_SAMPLER_MAG_FILTER[sampler->filter]);
//			glTexParameteri(glTarget, GL_TEXTURE_MIN_FILTER, crGL_SAMPLER_MIN_FILTER[sampler->filter]);
//			glTexParameteri(glTarget, GL_TEXTURE_WRAP_S, crGL_SAMPLER_ADDRESS[sampler->addressU]);
//			glTexParameteri(glTarget, GL_TEXTURE_WRAP_T, crGL_SAMPLER_ADDRESS[sampler->addressV]);
		}
		else
			glBindTexture(GL_TEXTURE_2D, 0);
	}

	checkError();

	return true;
}

static const StaticArray<GLenum, 8> _elementTypeMappings = {
	GLenum(0),
	GL_FLOAT,
	GL_BYTE,
	GL_UNSIGNED_BYTE,
	GL_SHORT,
	GL_UNSIGNED_SHORT,
	GL_INT,
	GL_UNSIGNED_INT
};

bool _bindShaderUniform(roRDriverShaderBufferInput* inputs, roSize inputCount, unsigned*)
{
	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_GL());
	if(!ctx) return false;
	roRDriverShaderProgramImpl* impl = static_cast<roRDriverShaderProgramImpl*>(ctx->currentShaderProgram);
	if(!impl) return false;

	checkError();

	// Make a hash value for the inputs
	unsigned inputHash = 0;
	for(unsigned attri=0; attri<inputCount; ++attri)
	{
		roRDriverShaderBufferInput* i = &inputs[attri];
		roRDriverBufferImpl* buffer = static_cast<roRDriverBufferImpl*>(i->buffer);
		if(!i || !i->buffer) continue;
		if(buffer->type != roRDriverBufferType_Vertex) continue;	// VAO only consider vertex buffer

		struct BlockToHash {
			void* systemBuf;
			GLuint vbo;
			unsigned stride, offset;
		};

		BlockToHash block = {
			buffer->systemBuf,
			buffer->glh,
			i->stride, i->offset
		};

		inputHash += _hash(&block, sizeof(block));	// We use += such that the order of inputs are not important
	}

	// Search for existing VAO
	GLuint vao = 0;
	bool vaoCacheFound = false;
	for(unsigned i=0; i<ctx->vaoCache.size(); ++i)
	{
		if(inputHash == ctx->vaoCache[i].hash) {
			vao = ctx->vaoCache[i].glh;
			ctx->vaoCache[i].lastUsedTime = ctx->lastSwapTime;
			vaoCacheFound = true;
			break;
		}
	}

	if(vao == 0) {
		glGenVertexArrays(1, &vao);
		VertexArrayObject o = { inputHash, vao, 1.0f };
		ctx->vaoCache.pushBack(o);
	}

	glBindVertexArray(vao);
	ctx->bindedIndexCount = 0;

	// Loop for each inputs and do the necessary binding
	for(unsigned attri=0; attri<inputCount; ++attri)
	{
		roRDriverShaderBufferInput* i = &inputs[attri];

		if(!i || !i->buffer)
			continue;

		roRDriverBufferImpl* buffer = static_cast<roRDriverBufferImpl*>(i->buffer);

		// Bind index buffer
		if(buffer->type == roRDriverBufferType_Index)
		{
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer->glh);
			ctx->currentIndexBufSysMemPtr = buffer->systemBuf;
			ctx->bindedIndexCount = buffer->sizeInBytes / sizeof(roUint16);
		}
		// Bind vertex buffer
		else if((buffer->type == roRDriverBufferType_Vertex) && !vaoCacheFound)
		{
			// Generate nameHash if necessary
			if(i->nameHash == 0 && i->name)
				i->nameHash = stringHash(i->name, 0);

			// See: http://www.opengl.org/sdk/docs/man/xhtml/glVertexAttribPointer.xml
			if(ProgramAttribute* a = _findProgramAttribute(ctx->currentShaderProgram, i->nameHash)) {
				// TODO: glVertexAttribPointer only work for vertex array but not plain system memory in certain GL version
				glEnableVertexAttribArray(a->location);
				glBindBuffer(GL_ARRAY_BUFFER, buffer->glh);
				glVertexAttribPointer(a->location, a->elementCount, a->elementType, false, i->stride, (void*)(ptrdiff_t(buffer->systemBuf) + i->offset));
				checkError();
			}
			else {
				roLog("error", "attribute '%s' not found!\n", i->name ? i->name : "");
				return false;
			}
		}
		// Bind uniforms
		else if(buffer->type == roRDriverBufferType_Uniform)
		{
			// Generate nameHash if necessary
			if(i->nameHash == 0 && i->name)
				i->nameHash = stringHash(i->name, 0);

			// NOTE: Shader compiler may optimize away the uniform
			roIgnoreRet(_setUniformBuffer(impl, i->nameHash, buffer, i));
		}
	}

	checkError();

	return true;
}

// ----------------------------------------------------------------------
// Making draw call

static const StaticArray<GLenum, 6> _primitiveTypeMappings = {
	GL_POINTS,
	GL_LINES,
	GL_LINE_STRIP,
	GL_TRIANGLES,
	GL_TRIANGLE_STRIP,
	GL_TRIANGLE_FAN
};

static void _drawPrimitive(roRDriverPrimitiveType type, roSize offset, roSize vertexCount, unsigned flags)
{
	GLenum mode = _primitiveTypeMappings[type];
	glDrawArrays(mode, num_cast<GLsizei>(offset), num_cast<GLsizei>(vertexCount));
}

static void _drawPrimitiveIndexed(roRDriverPrimitiveType type, roSize offset, roSize indexCount, unsigned flags)
{
	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_GL());
	if(!ctx) return;

	roAssert(indexCount <= ctx->bindedIndexCount);

	GLenum mode = _primitiveTypeMappings[type];
	GLenum indexType = GL_UNSIGNED_SHORT;
	ptrdiff_t byteOffset = offset * sizeof(roUint16);

	checkError();
	if(ctx->currentIndexBufSysMemPtr)
		byteOffset += ptrdiff_t(ctx->currentIndexBufSysMemPtr);

	glDrawElements(mode, num_cast<GLsizei>(indexCount), indexType, (void*)byteOffset);
	checkError();
}

static void _drawTriangle(roSize offset, roSize vertexCount, unsigned flags)
{
	_drawPrimitive(roRDriverPrimitiveType_TriangleList, offset, vertexCount, flags);
}

static void _drawTriangleIndexed(roSize offset, roSize indexCount, unsigned flags)
{
	_drawPrimitiveIndexed(roRDriverPrimitiveType_TriangleList, offset, indexCount, flags);
}


// ----------------------------------------------------------------------
// Driver

struct roRDriverImpl : public roRDriver
{
	String _driverName;
};	// roRDriver

static void _rhDeleteRenderDriver_GL(roRDriver* self)
{
	_allocator.deleteObj(static_cast<roRDriverImpl*>(self));
}

}	// namespace

roRDriver* _roNewRenderDriver_GL(const char* driverStr, const char*)
{
	roRDriverImpl* ret = _allocator.newObj<roRDriverImpl>().unref();
	roZeroMemory(ret, sizeof(*ret));
	ret->destructor = &_rhDeleteRenderDriver_GL;
	ret->_driverName = driverStr;
	ret->driverName = ret->_driverName.c_str();

	// Setup the function pointers
	ret->newContext = _newDriverContext_GL;
	ret->deleteContext = _deleteDriverContext_GL;
	ret->initContext = _initDriverContext_GL;
	ret->useContext = _useDriverContext_GL;
	ret->currentContext = _getCurrentContext_GL;
	ret->swapBuffers = _driverSwapBuffers_GL;
	ret->changeResolution = _driverChangeResolution_GL;
	ret->setViewport = _setViewport;
	ret->setScissorRect = _setScissorRect;
	ret->clearColor = _clearColor;
	ret->clearDepth = _clearDepth;
	ret->clearStencil = _clearStencil;

	ret->adjustDepthRangeMatrix = _adjustDepthRangeMatrix;
	ret->setRenderTargets = _setRenderTargets;

	ret->applyDefaultState = rgDriverApplyDefaultState;
	ret->setBlendState = _setBlendState;
	ret->setRasterizerState = _setRasterizerState;
	ret->setDepthStencilState = _setDepthStencilState;
	ret->setTextureState = _setTextureState;

	ret->newBuffer = _newBuffer;
	ret->deleteBuffer = _deleteBuffer;
	ret->initBuffer = _initBuffer;
	ret->updateBuffer = _updateBuffer;
	ret->resizeBuffer = _resizeBuffer;
	ret->mapBuffer = _mapBuffer;
	ret->unmapBuffer = _unmapBuffer;

	ret->newTexture = _newTexture;
	ret->deleteTexture = _deleteTexture;
	ret->initTexture = _initTexture;
	ret->updateTexture = _updateTexture;
	ret->mapTexture = _mapTexture;
	ret->unmapTexture = _unmapTexture;

	ret->newShader = _newShader;
	ret->deleteShader = _deleteShader;
	ret->initShader = _initShader;
	ret->initShaderFromBlob = _initShaderFromBlob;
	ret->deleteShaderBlob = _deleteShaderBlob;

	ret->bindShaders = _bindShaders;
	ret->bindShaderTextures = _bindShaderTexture;
	ret->bindShaderBuffers = _bindShaderUniform;

	ret->drawTriangle = _drawTriangle;
	ret->drawTriangleIndexed = _drawTriangleIndexed;
	ret->drawPrimitive = _drawPrimitive;
	ret->drawPrimitiveIndexed = _drawPrimitiveIndexed;

	return ret;
}
