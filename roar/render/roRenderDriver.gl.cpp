#include "pch.h"
#include "roRenderDriver.h"

#include "../base/roArray.h"
#include "../base/roLog.h"
#include "../base/roMemory.h"
#include "../base/roString.h"
#include "../base/roStringHash.h"

#include "../platform/roPlatformHeaders.h"
#include <gl/gl.h>
#include "../../src/render/gl/glext.h"
#include "platform.win/extensionsfwd.h"

// OpenGL stuffs
// Instancing:				http://sol.gfxile.net/instancing.html
// Pixel buffer object:		http://www.songho.ca/opengl/gl_pbo.html
// Silhouette shader:		http://prideout.net/blog/p54/Silhouette.glsl
// Buffer object streaming:	http://www.opengl.org/wiki/Buffer_Object_Streaming
// Geometry shader examples:http://renderingwonders.wordpress.com/tag/geometry-shader/
// Opengl on Mac:			http://developer.apple.com/library/mac/#documentation/GraphicsImaging/Conceptual/OpenGL-MacProgGuide/opengl_intro/opengl_intro.html

using namespace ro;

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

static unsigned _hash(const void* data, unsigned len)
{
	unsigned h = 0;
	const char* data_ = reinterpret_cast<const char*>(data);
	for(unsigned i=0; i<len; ++i)
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
	glEnable(GL_SCISSOR_TEST);
	glViewport((GLint)x, (GLint)y, (GLsizei)width, (GLsizei)height);
	glScissor((GLint)x, (GLint)y, (GLsizei)width, (GLsizei)height);
	glDepthRange(zmin, zmax);
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

// ----------------------------------------------------------------------
// State management

static const StaticArray<GLenum, 5> _blendOp = {
	GL_FUNC_ADD,
	GL_FUNC_SUBTRACT,
	GL_FUNC_REVERSE_SUBTRACT,
	GL_MIN,
	GL_MAX,
};

static const StaticArray<GLenum, 10> _blendValue = {
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
	if(state->hash == 0) {
		state->hash = (void*)_hash(
			&state->enable,
			sizeof(roRDriverBlendState) - offsetof(roRDriverBlendState, roRDriverBlendState::enable)
		);
		ctx->currentBlendStateHash = state->hash;
	}
	else if(state->hash == ctx->currentBlendStateHash)
		return;
	else {
		// TODO: Make use of the hash value, if OpenGL support state block
	}

	glColorMask(
		(state->wirteMask & roRDriverColorWriteMask_EnableRed) > 0,
		(state->wirteMask & roRDriverColorWriteMask_EnableGreen) > 0,
		(state->wirteMask & roRDriverColorWriteMask_EnableBlue) > 0,
		(state->wirteMask & roRDriverColorWriteMask_EnableAlpha) > 0
	);

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

static const StaticArray<GLenum, 8> _compareFunc = {
	GL_NEVER,
	GL_LESS,
	GL_EQUAL,
	GL_LEQUAL,
	GL_GREATER,
	GL_NOTEQUAL,
	GL_GEQUAL,
	GL_ALWAYS,
};

static const StaticArray<GLenum, 8> _stencilOp = {
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
			&state->enableDepth,
			sizeof(roRDriverDepthStencilState) - offsetof(roRDriverDepthStencilState, roRDriverDepthStencilState::enableDepth)
		);
		ctx->currentDepthStencilStateHash = state->hash;
	}
	else if(state->hash == ctx->currentDepthStencilStateHash)
		return;
	else {
		// TODO: Make use of the hash value, if OpenGL support state block
	}

	if(!state->enableDepth) {
		glDisable(GL_DEPTH_TEST);
	}
	else {
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(_compareFunc[state->depthFunc]);
	}

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

static const StaticArray<GLenum, 4> _magFilter = { GL_NEAREST, GL_LINEAR, GL_NEAREST, GL_LINEAR };
static const StaticArray<GLenum, 4> _minFilter = { GL_NEAREST, GL_LINEAR, GL_NEAREST_MIPMAP_NEAREST, GL_LINEAR_MIPMAP_LINEAR };
static const StaticArray<GLenum, 4> _textureAddressMode = { GL_REPEAT, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_BORDER, GL_MIRRORED_REPEAT_ARB };

// For OpenGl sampler object, see: http://www.geeks3d.com/20110908/opengl-3-3-sampler-objects-control-your-texture-units/
// and http://www.opengl.org/registry/specs/ARB/sampler_objects.txt
static void _setTextureState(roRDriverTextureState* states, unsigned stateCount, unsigned startingTextureUnit)
{
	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_GL());
	if(!ctx || !states || stateCount == 0) return;

	for(unsigned i=0; i<stateCount; ++i)
	{
		roRDriverTextureState& state = states[i];

		// Generate the hash value if not yet
		if(state.hash == 0) {
			state.hash = (void*)_hash(
				&state.filter,
				sizeof(roRDriverTextureState) - offsetof(roRDriverTextureState, roRDriverTextureState::filter)
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
			glSamplerParameterf(glh, GL_TEXTURE_MAX_ANISOTROPY_EXT, (float)state.maxAnisotropy);

			ctx->textureStateCache[freeCacheSlot].glh = glh;
			ctx->textureStateCache[freeCacheSlot].hash = state.hash;
		}

		glBindSampler(startingTextureUnit + i, glh);
	}
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
#endif

struct roRDriverBufferImpl : public roRDriverBuffer
{
	GLuint glh;
	void* systemBuf;
};	// roRDriverBufferImpl

static roRDriverBuffer* _newBuffer()
{
	roRDriverBufferImpl* ret = _allocator.newObj<roRDriverBufferImpl>().unref();
	memset(ret, 0, sizeof(*ret));
	return ret;
}

static void _deleteBuffer(roRDriverBuffer* self)
{
	roRDriverBufferImpl* impl = static_cast<roRDriverBufferImpl*>(self);
	if(!impl) return;

	if(impl->glh)
		glDeleteBuffers(1, &impl->glh);

	roAssert(!impl->isMapped);
	_allocator.free(impl->systemBuf);
	_allocator.deleteObj(static_cast<roRDriverBufferImpl*>(self));
}

static const StaticArray<GLenum, 4> _bufferUsage = {
	0,
	GL_STATIC_DRAW,
	GL_STREAM_DRAW,
	GL_DYNAMIC_DRAW
};

static bool _initBufferSpecificLocation(roRDriverBufferImpl* impl, roRDriverBufferType type, roRDriverDataUsage usage, void* initData, unsigned sizeInBytes, bool systemMemory)
{
	impl->type = type;
	impl->sizeInBytes = sizeInBytes;
	impl->usage = usage;

	checkError();

	if(systemMemory) {
		impl->systemBuf = _allocator.malloc(sizeInBytes);
		if(initData)
 			memcpy(impl->systemBuf, initData, sizeInBytes);

		if(impl->glh) {
			impl->isMapped = false;	// Calling glDeleteBuffers also implies glUnmapBuffer
			glDeleteBuffers(1, &impl->glh);
			impl->glh = 0;
		}
 	}
 	else
	{
		glGenBuffers(1, &impl->glh);
		GLenum t = _bufferTarget[type];
		roAssert("Invalid roRDriverBufferType" && t != 0);
		glBindBuffer(t, impl->glh);
		glBufferData(t, sizeInBytes, initData, _bufferUsage[usage]);
		roAssert(impl->glh);

		_allocator.free(impl->systemBuf);
	}

	checkError();

	return true;
}

static bool _initBuffer(roRDriverBuffer* self, roRDriverBufferType type, roRDriverDataUsage usage, void* initData, unsigned sizeInBytes)
{
	roRDriverBufferImpl* impl = static_cast<roRDriverBufferImpl*>(self);
	if(!impl) return false;

	roAssert(!impl->glh && !impl->systemBuf);
	return _initBufferSpecificLocation(impl, type, usage, initData, sizeInBytes, false);
}

static bool _updateBuffer(roRDriverBuffer* self, unsigned offsetInBytes, void* data, unsigned sizeInBytes)
{
	roRDriverBufferImpl* impl = static_cast<roRDriverBufferImpl*>(self);
	if(!impl) return false;

	if(!data) return false;
	if(impl->isMapped) return false;
	if(offsetInBytes + sizeInBytes > self->sizeInBytes) return false;
	if(impl->usage == roRDriverDataUsage_Static) return false;

 	if(impl->systemBuf)
 		memcpy(((char*)impl->systemBuf) + offsetInBytes, data, sizeInBytes);
 	else {
		checkError();
		GLenum t = _bufferTarget[self->type];
		glBindBuffer(t, impl->glh);
		glBufferSubData(t, offsetInBytes, sizeInBytes, data);
		checkError();
	}

	return true;
}

static void* _mapBuffer(roRDriverBuffer* self, roRDriverBufferMapUsage usage)
{
	roRDriverBufferImpl* impl = static_cast<roRDriverBufferImpl*>(self);
	if(!impl) return NULL;

	if(impl->systemBuf)
		return impl->systemBuf;

	if(impl->isMapped) return NULL;

#if !defined(RG_GLES)
	void* ret = NULL;
	checkError();
	roAssert("Invalid roRDriverBufferMapUsage" && _bufferMapUsage[usage] != 0);
	GLenum t = _bufferTarget[self->type];
	glBindBuffer(t, impl->glh);

	// The write discard optimization
	if(!usage & roRDriverBufferMapUsage_Read)
		glBufferData(t, 0, NULL, GL_STREAM_COPY);

	ret = glMapBuffer(t, _bufferMapUsage[usage]);
	checkError();

	impl->isMapped = true;
	impl->mapUsage = usage;

	return ret;
#else
	return NULL;
#endif
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
	checkError();
#endif

	impl->isMapped = false;
}

static bool _switchBufferMode(roRDriverBufferImpl* impl)
{
	roAssert((impl->glh != 0) != (impl->systemBuf != NULL) && "Only glh or system buffer but not using both");

	if(impl->glh) {
		if(void* data = _mapBuffer(impl, roRDriverBufferMapUsage_Read))
			return _initBufferSpecificLocation(impl, impl->type, impl->usage, data, impl->sizeInBytes, true);
	}
	else if(impl->systemBuf) {
		return _initBufferSpecificLocation(impl, impl->type, impl->usage, impl->systemBuf, impl->sizeInBytes, false);
	}

	return false;
}

// ----------------------------------------------------------------------
// Texture

typedef struct TextureFormatMapping
{
	roRDriverTextureFormat format;
	unsigned glPixelSize;
	GLint glInternalFormat;	// eg. GL_RGBA8
	GLenum glFormat;		// eg. GL_RGBA
	GLenum glType;			// eg. GL_UNSIGNED_BYTE
} TextureFormatMapping;

TextureFormatMapping _textureFormatMappings[] = {
	{ roRDriverTextureFormat(0),			0, 0, 0, 0 },
	{ roRDriverTextureFormat_RGBA,			4, GL_RGBA8, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8 },
	{ roRDriverTextureFormat_R,				1, GL_R8, GL_RED, GL_UNSIGNED_BYTE },
	{ roRDriverTextureFormat_A,				1, GL_ALPHA, GL_ALPHA, GL_UNSIGNED_BYTE },
	{ roRDriverTextureFormat_Depth,			0, 0, 0, 0 },
	{ roRDriverTextureFormat_DepthStencil,	0, 0, 0, 0 },
	{ roRDriverTextureFormat_PVRTC2,		0, 0, 0, 0 },
	{ roRDriverTextureFormat_PVRTC4,		0, 0, 0, 0 },
//	{ roRDriverTextureFormat_PVRTC2,		2, GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG, 0, 0 },
//	{ roRDriverTextureFormat_PVRTC4,		1, GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG, 0, 0 },
	{ roRDriverTextureFormat_DXT1,			0, 0, 0, 0 },
	{ roRDriverTextureFormat_DXT5,			0, 0, 0, 0 },
};

struct roRDriverTextureImpl : public roRDriverTexture
{
	GLuint glh;
	GLenum glTarget;	// eg. GL_TEXTURE_2D
	TextureFormatMapping* formatMapping;
};	// roRDriverTextureImpl

static roRDriverTexture* _newTexture()
{
	roRDriverTextureImpl* ret = _allocator.newObj<roRDriverTextureImpl>().unref();
	memset(ret, 0, sizeof(*ret));
	return ret;
}

static void _deleteTexture(roRDriverTexture* self)
{
	roRDriverTextureImpl* impl = static_cast<roRDriverTextureImpl*>(self);
	if(!impl) return;

	if(impl->glh)
		glDeleteTextures(1, &impl->glh);

	_allocator.deleteObj(static_cast<roRDriverTextureImpl*>(self));
}

static bool _initTexture(roRDriverTexture* self, unsigned width, unsigned height, roRDriverTextureFormat format)
{
	roRDriverTextureImpl* impl = static_cast<roRDriverTextureImpl*>(self);
	if(!impl) return false;
	if(impl->format || impl->glh) return false;

	impl->width = width;
	impl->height = height;
	impl->format = format;
	impl->glTarget = GL_TEXTURE_2D;
	impl->formatMapping = &(_textureFormatMappings[format]);

	return true;
}

static unsigned _mipLevelOffset(roRDriverTextureImpl* self, unsigned mipIndex, unsigned& mipWidth, unsigned& mipHeight)
{
	unsigned i = 0;
	unsigned offset = 0;

	mipWidth = self->width;
	mipHeight = self->height;

	for(unsigned i=0; i<mipIndex; ++i) {
		unsigned mipSize = mipWidth * mipHeight;

		if(roRDriverTextureFormat_Compressed & self->format)
			mipSize = mipSize >> self->formatMapping->glPixelSize;
		else
			mipSize = mipSize * self->formatMapping->glPixelSize;

		offset += mipSize;
		if(mipWidth > 1) mipWidth /= 2;
		if(mipHeight > 1) mipHeight /= 2;
	} while(++i < mipIndex);

	return offset;
}

static unsigned char* _mipLevelData(roRDriverTextureImpl* self, unsigned mipIndex, unsigned& mipWidth, unsigned& mipHeight, const void* data)
{
	mipWidth = self->width;
	mipHeight = self->height;

	if(!data)
		return NULL;

	return (unsigned char*)(data) + _mipLevelOffset(self, mipIndex, mipWidth, mipHeight);
}

static unsigned _textureSize(roRDriverTextureImpl* self, unsigned mipCount)
{
	unsigned ret = 0;
	unsigned w, h;
	for(unsigned i=0; i<mipCount; ++i) {
		ret += _mipLevelOffset(self, i+1, w, h);
	}
	return ret;
}

static bool _commitTexture(roRDriverTexture* self, const void* data, unsigned rowPaddingInBytes)
{
	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_GL());
	roRDriverTextureImpl* impl = static_cast<roRDriverTextureImpl*>(self);
	if(!ctx || !impl) return false;
	if(!impl->format) return false;

	checkError();

	if(!impl->glh)
		glGenTextures(1, &impl->glh);

	const unsigned mipCount = 1;	// TODO: Allow loading mip maps

#if !defined(RG_GLES)
	// Using PBO
	static const bool usePbo = true;
	if(usePbo) {
		unsigned textureSize = _textureSize(impl, mipCount);
		GLuint pbo = ctx->pixelBufferCache[(ctx->currentPixelBufferIndex++) % ctx->pixelBufferCache.size()];
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
		glBufferData(GL_PIXEL_UNPACK_BUFFER, textureSize, data, GL_STREAM_DRAW);
	}
#else
	static const bool usePbo = false;
#endif

	glBindTexture(impl->glTarget, impl->glh);

	// Give the texture object a valid sampler state, even we might use sampler object
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	TextureFormatMapping* mapping = impl->formatMapping;

	for(unsigned i=0; i<mipCount; ++i)
	{
		unsigned mipw, miph;
		unsigned char* mipData = _mipLevelData(impl, i, mipw, miph, usePbo ? NULL : data);

		if(impl->format & roRDriverTextureFormat_Compressed) {
			unsigned imgSize = (mipw * miph) >> mapping->glPixelSize;
			glCompressedTexImage2D(
				impl->glTarget, i, mapping->glInternalFormat, 
				mipw, miph, 0,
				imgSize,
				mipData
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
					impl->glTarget, i, mapping->glInternalFormat,
					mipw, miph, 0,
					mapping->glFormat, mapping->glType,
					mipData
				);
			}
			else {
				// Create an empty texture object first
				glTexImage2D(
					impl->glTarget, i, mapping->glInternalFormat,
					mipw, miph, 0,
					mapping->glFormat, mapping->glType,
					NULL
				);

				// Then fill the pixels row by row
				for(unsigned y=0; y<miph; ++y) {
					const unsigned char* row = mipData + y * (mipw * mapping->glPixelSize + rowPaddingInBytes);
					glTexSubImage2D(
						impl->glTarget, i, 0, y,
						mipw, 1,
						mapping->glFormat, mapping->glType,
						row
					);
				}
			}

			glPixelStorei(GL_UNPACK_ALIGNMENT, alignmentBackup);
		}
	}

#if !defined(RG_GLES)
	if(usePbo)
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
#endif

	checkError();

	return true;
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
	memset(ret, 0, sizeof(*ret));
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

		roRDriverShaderImpl** cache = arrayFind(
			program.shaders.begin(),
			program.shaders.end(),
			impl
		);

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

static bool _initShader(roRDriverShader* self, roRDriverShaderType type, const char** sources, unsigned sourceCount)
{
	roRDriverShaderImpl* impl = static_cast<roRDriverShaderImpl*>(self);
	if(!impl || sourceCount == 0) return false;

	checkError();

	self->type = type;
	impl->glh = glCreateShader(_shaderTypes[type]);

	glShaderSource(impl->glh, sourceCount, sources, NULL);
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

static ProgramUniform* _findProgramUniform(unsigned nameHash)
{
	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_GL());
	if(!ctx) return NULL;

	roRDriverShaderProgramImpl* impl = ctx->currentShaderProgram;
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

static bool _initShaderProgram(roRDriverShaderProgram* self, roRDriverShader** shaders, unsigned shaderCount)
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

		if(impl->uniforms.capacity() < (unsigned)uniformCount)
			roLog("verbose", "dynamic memory need for TinyArray in %s %d\n", __FILE__, __LINE__);

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

		if(impl->attributes.capacity() < (unsigned)attributeCount)
			roLog("verbose", "dynamic memory need for TinyArray in %s %d\n", __FILE__, __LINE__);

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

bool _bindShaders(roRDriverShader** shaders, unsigned shaderCount)
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
bool _setUniformBuffer(unsigned nameHash, roRDriverBuffer* buffer, roRDriverShaderInput* input)
{
	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_GL());
	if(!ctx) return false;

	roRDriverShaderProgramImpl* program = ctx->currentShaderProgram;
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

		// Switch to buffer object mode
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

	if(ProgramUniform* uniform = _findProgramUniform(nameHash)) {
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

bool _setUniformTexture(unsigned nameHash, roRDriverTexture* texture)
{
	ProgramUniform* uniform = _findProgramUniform(nameHash);
	if(!uniform ) return false;

	checkError();

	if(texture) {
		glActiveTexture(GL_TEXTURE0 + uniform->texunit);
		GLenum glTarget = static_cast<roRDriverTextureImpl*>(texture)->glTarget;
		GLuint glh = static_cast<roRDriverTextureImpl*>(texture)->glh;

		glBindTexture(glTarget, glh);
//		glTexParameteri(glTarget, GL_TEXTURE_MAG_FILTER, crGL_SAMPLER_MAG_FILTER[sampler->filter]);
//		glTexParameteri(glTarget, GL_TEXTURE_MIN_FILTER, crGL_SAMPLER_MIN_FILTER[sampler->filter]);
//		glTexParameteri(glTarget, GL_TEXTURE_WRAP_S, crGL_SAMPLER_ADDRESS[sampler->addressU]);
//		glTexParameteri(glTarget, GL_TEXTURE_WRAP_T, crGL_SAMPLER_ADDRESS[sampler->addressV]);
	}
	else
		glBindTexture(GL_TEXTURE_2D, 0);

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

bool _bindShaderInput(roRDriverShaderInput* inputs, unsigned inputCount, unsigned*)
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
		roRDriverShaderInput* i = &inputs[attri];
		roRDriverBufferImpl* buffer = static_cast<roRDriverBufferImpl*>(i->buffer);
		roRDriverShaderImpl* shader = static_cast<roRDriverShaderImpl*>(i->shader);
		if(!i || !i->buffer || !shader) continue;
		if(buffer->type != roRDriverBufferType_Vertex) continue;	// VAO only consider vertex buffer

		struct BlockToHash {
			void* systemBuf;
			GLuint shader, vbo;
			unsigned stride, offset;
		};

		BlockToHash block = {
			buffer->systemBuf,
			shader->glh, buffer->glh,
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
			ctx->vaoCache[i].hotness += 1.0f;
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

	// Loop for each inputs and do the necessary binding
	for(unsigned attri=0; attri<inputCount; ++attri)
	{
		roRDriverShaderInput* i = &inputs[attri];

		if(!i || !i->buffer)
			continue;

		roRDriverBufferImpl* buffer = static_cast<roRDriverBufferImpl*>(i->buffer);

		// Bind index buffer
		if(buffer->type == roRDriverBufferType_Index)
		{
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer->glh);
			ctx->currentIndexBufSysMemPtr = buffer->systemBuf;
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
			}
			else {
				roLog("error", "attribute not found!\n");
				return false;
			}
		}
		// Bind uniforms
		else if(buffer->type == roRDriverBufferType_Uniform)
		{
			// Generate nameHash if necessary
			if(i->nameHash == 0 && i->name)
				i->nameHash = stringHash(i->name, 0);

			if(!_setUniformBuffer(i->nameHash, buffer, i))
				return false;
		}
	}

	checkError();

	return true;
}

bool _bindShaderInputCached(unsigned)
{
	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_GL());
	if(!ctx) return false;
	roRDriverShaderProgramImpl* impl = static_cast<roRDriverShaderProgramImpl*>(ctx->currentShaderProgram);
	if(!impl) return false;

	// TODO: Implement
	return false;
}

// ----------------------------------------------------------------------
// Making draw call

static void _drawTriangle(unsigned offset, unsigned vertexCount, unsigned flags)
{
	GLenum mode = GL_TRIANGLES;
	glDrawArrays(mode, offset, vertexCount);
}

static void _drawTriangleIndexed(unsigned offset, unsigned indexCount, unsigned flags)
{
	roRDriverContextImpl* ctx = static_cast<roRDriverContextImpl*>(_getCurrentContext_GL());
	if(!ctx) return;

	GLenum mode = GL_TRIANGLES;
	GLenum indexType = GL_UNSIGNED_SHORT;
	ptrdiff_t byteOffset = offset * sizeof(roUint16);

	checkError();
	if(ctx->currentIndexBufSysMemPtr)
		byteOffset += ptrdiff_t(ctx->currentIndexBufSysMemPtr);

	glDrawElements(mode, indexCount, indexType, (void*)byteOffset);
	checkError();
}

// ----------------------------------------------------------------------
// Driver

struct roRDriverImpl : public roRDriver
{
};	// roRDriver

static void _rhDeleteRenderDriver_GL(roRDriver* self)
{
	_allocator.deleteObj(static_cast<roRDriverImpl*>(self));
}

}	// namespace

roRDriver* _rgNewRenderDriver_GL(const char*)
{
	roRDriverImpl* ret = _allocator.newObj<roRDriverImpl>().unref();
	memset(ret, 0, sizeof(*ret));
	ret->destructor = &_rhDeleteRenderDriver_GL;

	// Setup the function pointers
	ret->newContext = _newDriverContext_GL;
	ret->deleteContext = _deleteDriverContext_GL;
	ret->initContext = _initDriverContext_GL;
	ret->useContext = _useDriverContext_GL;
	ret->currentContext = _getCurrentContext_GL;
	ret->swapBuffers = _driverSwapBuffers_GL;
	ret->changeResolution = _driverChangeResolution_GL;
	ret->setViewport = _setViewport;
	ret->clearColor = _clearColor;
	ret->clearDepth = _clearDepth;
	ret->clearStencil = _clearStencil;

	ret->adjustDepthRangeMatrix = _adjustDepthRangeMatrix;

	ret->applyDefaultState = rgDriverApplyDefaultState;
	ret->setBlendState = _setBlendState;
	ret->setDepthStencilState = _setDepthStencilState;
	ret->setTextureState = _setTextureState;

	ret->newBuffer = _newBuffer;
	ret->deleteBuffer = _deleteBuffer;
	ret->initBuffer = _initBuffer;
	ret->updateBuffer = _updateBuffer;
	ret->mapBuffer = _mapBuffer;
	ret->unmapBuffer = _unmapBuffer;

	ret->newTexture = _newTexture;
	ret->deleteTexture = _deleteTexture;
	ret->initTexture = _initTexture;
	ret->commitTexture = _commitTexture;

	ret->newShader = _newShader;
	ret->deleteShader = _deleteShader;
	ret->initShader = _initShader;

	ret->bindShaders = _bindShaders;
	ret->setUniformTexture = _setUniformTexture;

	ret->bindShaderInput = _bindShaderInput;
	ret->bindShaderInputCached = _bindShaderInputCached;

	ret->drawTriangle = _drawTriangle;
	ret->drawTriangleIndexed = _drawTriangleIndexed;

	return ret;
}
