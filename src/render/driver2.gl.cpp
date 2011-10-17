#include "pch.h"
#include "driver2.h"

#include <gl/gl.h>
#include "gl/glext.h"
#include "win32/extensionswin32fwd.h"

#include "../array.h"
#include "../common.h"
#include "../rhlog.h"
#include "../vector.h"
#include "../rhstring.h"

//////////////////////////////////////////////////////////////////////////
// Common stuffs

#define checkError() { GLenum err = glGetError(); if(err != GL_NO_ERROR) rhLog("error", "unhandled GL error 0x%04x before %s %d\n", err, __FILE__, __LINE__); }
#define clearError() { glGetError(); }

//////////////////////////////////////////////////////////////////////////
// Context management

// These functions are implemented in platform specific src files, eg. driver2.gl.windows.cpp
extern RgDriverContext* _newDriverContext_GL();
extern void _deleteDriverContext_GL(RgDriverContext* self);
extern bool _initDriverContext_GL(RgDriverContext* self, void* platformSpecificWindow);
extern void _useDriverContext_GL(RgDriverContext* self);
extern RgDriverContext* _getCurrentContext_GL();

extern void _driverSwapBuffers_GL();
extern bool _driverChangeResolution_GL(unsigned width, unsigned height);

namespace {

struct RgDriverContextImpl : public RgDriverContext
{
	unsigned currentBlendStateHash;
	unsigned currentDepthStencilStateHash;

	struct TextureState {
		unsigned hash;
		GLuint glh;
	};
	Array<TextureState, 64> textureStateCache;
};	// RgDriverContextImpl

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
	glClearDepth(z);
	glClear(GL_DEPTH_BUFFER_BIT);
}

static void _clearStencil(unsigned char s)
{
	glClearStencil(s);
	glClear(GL_STENCIL_BUFFER_BIT);
}

//////////////////////////////////////////////////////////////////////////
// State management

static unsigned _hash(const void* data, unsigned len)
{
	unsigned h = 0;
	const char* data_ = reinterpret_cast<const char*>(data);
	for(unsigned i=0; i<len; ++i)
		h = data_[i] + (h << 6) + (h << 16) - h;
	return h;
}

static const Array<GLenum, 5> _blendOp = {
	GL_FUNC_ADD,
	GL_FUNC_SUBTRACT,
	GL_FUNC_REVERSE_SUBTRACT,
	GL_MIN,
	GL_MAX,
};

static const Array<GLenum, 10> _blendValue = {
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
static void _setBlendState(RgDriverBlendState* state)
{
	RgDriverContextImpl* ctx = reinterpret_cast<RgDriverContextImpl*>(_getCurrentContext_GL());
	if(!state || !ctx) return;

	// Generate the hash value if not yet
	if(state->hash == 0) {
		state->hash = _hash(
			&state->enable,
			sizeof(RgDriverBlendState) - offsetof(RgDriverBlendState, RgDriverBlendState::enable)
		);
		ctx->currentBlendStateHash = state->hash;
	}
	else if(state->hash == ctx->currentBlendStateHash)
		return;
	else {
		// TODO: Make use of the hash value, if OpenGL support state block
	}

	glColorMask(
		(state->wirteMask & RgDriverColorWriteMask_EnableRed) > 0,
		(state->wirteMask & RgDriverColorWriteMask_EnableGreen) > 0,
		(state->wirteMask & RgDriverColorWriteMask_EnableBlue) > 0,
		(state->wirteMask & RgDriverColorWriteMask_EnableAlpha) > 0
	);

	if(state->enable)
		glEnable(GL_BLEND);
	else {
		glDisable(GL_BLEND);
		return;
	}

	glBlendEquationSeparate(
		_blendOp[state->colorOp],
		_blendOp[state->alphaOp]
	);

/*	glBlendFunc(
		_blendValue[state->colorSrc],
		_blendValue[state->colorDst]
	);*/
	glBlendFuncSeparate(
		_blendValue[state->colorSrc],
		_blendValue[state->colorDst],
		_blendValue[state->alphaSrc],
		_blendValue[state->alphaDst]
	);
}

static const Array<GLenum, 8> _compareFunc = {
	GL_NEVER,
	GL_LESS,
	GL_EQUAL,
	GL_LEQUAL,
	GL_GREATER,
	GL_NOTEQUAL,
	GL_GEQUAL,
	GL_ALWAYS,
};

static const Array<GLenum, 8> _stencilOp = {
	GL_ZERO,
	GL_INVERT,
	GL_KEEP,
	GL_REPLACE,
	GL_INCR,
	GL_DECR,
	GL_INCR_WRAP,
	GL_DECR_WRAP,
};

static void _setDepthStencilState(RgDriverDepthStencilState* state)
{
	RgDriverContextImpl* ctx = reinterpret_cast<RgDriverContextImpl*>(_getCurrentContext_GL());
	if(!state || !ctx) return;

	// Generate the hash value if not yet
	if(state->hash == 0) {
		state->hash = _hash(
			&state->enableDepth,
			sizeof(RgDriverDepthStencilState) - offsetof(RgDriverDepthStencilState, RgDriverDepthStencilState::enableDepth)
		);
		ctx->currentDepthStencilStateHash = state->hash;
	}
	else if(state->hash == ctx->currentDepthStencilStateHash)
		return;
	else {
		// TODO: Make use of the hash value, if OpenGL support state block
	}

	if(!state->enableStencil) {
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
			state->front.func,
			state->front.refValue,
			state->front.mask
		);
		glStencilFuncSeparate(
			GL_BACK,
			state->back.func,
			state->back.refValue,
			state->back.mask
		);

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

static const Array<GLenum, 4> _magFilter = { GL_NEAREST, GL_LINEAR, GL_NEAREST, GL_LINEAR };
static const Array<GLenum, 4> _minFilter = { GL_NEAREST, GL_LINEAR, GL_NEAREST_MIPMAP_NEAREST, GL_LINEAR_MIPMAP_LINEAR };
static const Array<GLenum, 4> _textureAddressMode = { GL_REPEAT, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_BORDER, GL_MIRRORED_REPEAT_ARB };

// For OpenGl sampler object, see: http://www.geeks3d.com/20110908/opengl-3-3-sampler-objects-control-your-texture-units/
// and http://www.opengl.org/registry/specs/ARB/sampler_objects.txt
static void _setTextureState(RgDriverTextureState* states, unsigned stateCount, unsigned startingTextureUnit)
{
	RgDriverContextImpl* ctx = reinterpret_cast<RgDriverContextImpl*>(_getCurrentContext_GL());
	if(!ctx || !states || stateCount == 0) return;

	for(unsigned i=0; i<stateCount; ++i)
	{
		RgDriverTextureState& state = states[i];

		// Generate the hash value if not yet
		if(state.hash == 0) {
			state.hash = _hash(
				&state.filter,
				sizeof(RgDriverTextureState) - offsetof(RgDriverTextureState, RgDriverTextureState::filter)
			);
		}

		GLuint glh = 0;
		int freeCacheSlot = -1;

		// Try to search for the state in the cache
		for(unsigned j=0; j<ctx->textureStateCache.size(); ++j) {
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
				rhLog("error", "RgDriver texture cache slot full\n");
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

//////////////////////////////////////////////////////////////////////////
// Buffer

static const Array<GLenum, 9> _bufferTarget = {
	0,
	GL_ARRAY_BUFFER,
	GL_ELEMENT_ARRAY_BUFFER, 0,
#if !defined(CR_GLES_2)
	GL_UNIFORM_BUFFER, 0, 0, 0, 0
#endif
};

#if !defined(CR_GLES_2)
static const Array<GLenum, 4> _bufferMapUsage = {
	0,
	GL_READ_ONLY,
	GL_WRITE_ONLY,
	GL_READ_WRITE,
};
#endif

struct RgDriverBufferImpl : public RgDriverBuffer
{
	GLuint glh;
	void* systemBuf;
};	// RgDriverBufferImpl

static RgDriverBuffer* _newBuffer()
{
	RgDriverBufferImpl* ret = new RgDriverBufferImpl;
	memset(ret, 0, sizeof(*ret));
	return ret;
}

static void _deleteBuffer(RgDriverBuffer* self)
{
	RgDriverBufferImpl* impl = static_cast<RgDriverBufferImpl*>(self);
	if(!impl) return;

	if(impl->glh)
		glDeleteBuffers(1, &impl->glh);

	rhinoca_free(impl->systemBuf);
	delete static_cast<RgDriverBufferImpl*>(self);
}

static bool _initBuffer(RgDriverBuffer* self, RgDriverBufferType type, void* initData, unsigned sizeInBytes)
{
	RgDriverBufferImpl* impl = static_cast<RgDriverBufferImpl*>(self);
	if(!impl) return false;

	self->type = type;
	self->sizeInBytes = sizeInBytes;

	if(type & RgDriverBufferType_System) {
		impl->systemBuf = rhinoca_malloc(sizeInBytes);
		if(initData)
			memcpy(impl->systemBuf, initData, sizeInBytes);
	}
	else {
		checkError();
		glGenBuffers(1, &impl->glh);
		RHASSERT("Invalid RgDriverBufferType" && _bufferTarget[type] != 0);
		glBindBuffer(_bufferTarget[type], impl->glh);
		glBufferData(_bufferTarget[type], sizeInBytes, initData, GL_STREAM_DRAW);
		RHASSERT(impl->glh);
		checkError();
	}

	return true;
}

static bool _updateBuffer(RgDriverBuffer* self, unsigned offsetInBytes, void* data, unsigned sizeInBytes)
{
	RgDriverBufferImpl* impl = static_cast<RgDriverBufferImpl*>(self);
	if(!impl) return false;

	if(offsetInBytes + sizeInBytes > self->sizeInBytes)
		return false;

	if(self->type & RgDriverBufferType_System)
		memcpy(((char*)impl->systemBuf) + offsetInBytes, data, sizeInBytes);
	else {
		checkError();
		glBindBuffer(_bufferTarget[self->type], impl->glh);
		glBufferSubData(_bufferTarget[self->type], offsetInBytes, sizeInBytes, data);
		checkError();
	}

	return true;
}

static void* _mapBuffer(RgDriverBuffer* self, RgDriverBufferMapUsage usage)
{
	RgDriverBufferImpl* impl = static_cast<RgDriverBufferImpl*>(self);
	if(!impl) return false;

	if(impl->systemBuf)
		return impl->systemBuf;

#if !defined(CR_GLES_2)
	void* ret = NULL;
	checkError();
	RHASSERT("Invalid RgDriverBufferMapUsage" && _bufferMapUsage[usage] != 0);
	glBindBuffer(_bufferTarget[self->type], impl->glh);
	ret = glMapBuffer(_bufferTarget[self->type], _bufferMapUsage[usage]);
	checkError();
	return ret;
#endif

	return NULL;
}

static void _unmapBuffer(RgDriverBuffer* self)
{
	RgDriverBufferImpl* impl = static_cast<RgDriverBufferImpl*>(self);
	if(!impl) return;

	if(impl->systemBuf) {
		// Nothing need to do to un-map the system memory
	}

#if !defined(CR_GLES_2)
	checkError();
	glBindBuffer(_bufferTarget[self->type], impl->glh);
	glUnmapBuffer(_bufferTarget[self->type]);
	checkError();
#endif
}

//////////////////////////////////////////////////////////////////////////
// Texture

typedef struct TextureFormatMapping
{
	RgDriverTextureFormat format;
	unsigned glPixelSize;
	GLint glInternalFormat;	// eg. GL_RGBA8
	GLenum glFormat;		// eg. GL_RGBA
	GLenum glType;			// eg. GL_UNSIGNED_BYTE
} TextureFormatMapping;

TextureFormatMapping _textureFormatMappings[] = {
	{ RgDriverTextureFormat(0),				0, 0, 0, 0 },
	{ RgDriverTextureFormat_RGBA,			4, GL_RGBA8, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8 },
	{ RgDriverTextureFormat_R,				1, GL_R8, GL_RED, GL_UNSIGNED_BYTE },
	{ RgDriverTextureFormat_A,				1, GL_ALPHA, GL_ALPHA, GL_UNSIGNED_BYTE },
	{ RgDriverTextureFormat_Depth,			0, 0, 0, 0 },
	{ RgDriverTextureFormat_DepthStencil,	0, 0, 0, 0 },
	{ RgDriverTextureFormat_PVRTC2,			0, 0, 0, 0 },
	{ RgDriverTextureFormat_PVRTC4,			0, 0, 0, 0 },
//	{ RgDriverTextureFormat_PVRTC2,			2, GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG, 0, 0 },
//	{ RgDriverTextureFormat_PVRTC4,			1, GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG, 0, 0 },
	{ RgDriverTextureFormat_DXT1,			0, 0, 0, 0 },
	{ RgDriverTextureFormat_DXT5,			0, 0, 0, 0 },
};

struct RgDriverTextureImpl : public RgDriverTexture
{
	GLuint glh;
	GLenum glTarget;	// eg. GL_TEXTURE_2D
	TextureFormatMapping* formatMapping;
};	// RgDriverTextureImpl

RgDriverTexture* _newTexture()
{
	RgDriverTextureImpl* ret = new RgDriverTextureImpl;
	memset(ret, 0, sizeof(*ret));
	return ret;
}

void _deleteTexture(RgDriverTexture* self)
{
	RgDriverTextureImpl* impl = static_cast<RgDriverTextureImpl*>(self);
	if(!impl) return;

	if(impl->glh)
		glDeleteTextures(1, &impl->glh);

	delete static_cast<RgDriverTextureImpl*>(self);
}

bool _initTexture(RgDriverTexture* self, unsigned width, unsigned height, RgDriverTextureFormat format)
{
	RgDriverTextureImpl* impl = static_cast<RgDriverTextureImpl*>(self);
	if(!impl) return false;
	if(impl->format || impl->glh) return false;

	impl->width = width;
	impl->height = height;
	impl->format = format;
	impl->glTarget = GL_TEXTURE_2D;
	impl->formatMapping = &(_textureFormatMappings[format]);

	return true;
}

unsigned _mipLevelOffset(RgDriverTextureImpl* self, unsigned mipIndex, unsigned& mipWidth, unsigned& mipHeight)
{
	unsigned i = 0;
	unsigned offset = 0;

	mipWidth = self->width;
	mipHeight = self->height;

	for(unsigned i=0; i<mipIndex; ++i) {
		unsigned mipSize = mipWidth * mipHeight;

		if(RgDriverTextureFormat_Compressed & self->format)
			mipSize = mipSize >> self->formatMapping->glPixelSize;
		else
			mipSize = mipSize * self->formatMapping->glPixelSize;

		offset += mipSize;
		if(mipWidth > 1) mipWidth /= 2;
		if(mipHeight > 1) mipHeight /= 2;
	} while(++i < mipIndex);

	return offset;
}

unsigned char* _mipLevelData(RgDriverTextureImpl* self, unsigned mipIndex, unsigned& mipWidth, unsigned& mipHeight, const void* data)
{
	mipWidth = self->width;
	mipHeight = self->height;

	if(!data)
		return NULL;

	return (unsigned char*)(data) + _mipLevelOffset(self, mipIndex, mipWidth, mipHeight);
}

bool _commitTexture(RgDriverTexture* self, const void* data, unsigned rowPaddingInBytes)
{
	RgDriverTextureImpl* impl = static_cast<RgDriverTextureImpl*>(self);
	if(!impl) return false;
	if(!impl->format) return false;

	checkError();

	glGenTextures(1, &impl->glh);
	glBindTexture(impl->glTarget, impl->glh);

	// Give the texture object a valid sampler state, even we might use sampler object
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	const unsigned mipCount = 1;	// TODO: Allow loading mip maps
	TextureFormatMapping* mapping = impl->formatMapping;

	for(unsigned i=0; i<mipCount; ++i)
	{
		unsigned mipw, miph;
		unsigned char* mipData = _mipLevelData(impl, i, mipw, miph, data);

		if(impl->format & RgDriverTextureFormat_Compressed) {
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

	checkError();

	return true;
}

//////////////////////////////////////////////////////////////////////////
// Shader

static const Array<GLenum, 5> _shaderTypes = {
	GL_VERTEX_SHADER,
	GL_FRAGMENT_SHADER,
#if !defined(CR_GLES_2)
	GL_GEOMETRY_SHADER,
	GL_TESS_CONTROL_SHADER,
	GL_TESS_EVALUATION_SHADER,
#endif
};

struct RgDriverShaderImpl : public RgDriverShader
{
	GLuint glh;
};	// RgDriverShaderImpl

static RgDriverShader* _newShader()
{
	RgDriverShaderImpl* ret = new RgDriverShaderImpl;
	memset(ret, 0, sizeof(*ret));
	return ret;
}

static void _deleteShader(RgDriverShader* self)
{
	RgDriverShaderImpl* impl = static_cast<RgDriverShaderImpl*>(self);
	if(!impl) return;

	if(impl->glh)
		glDeleteShader(impl->glh);

	delete static_cast<RgDriverShaderImpl*>(self);
}

static bool _initShader(RgDriverShader* self, RgDriverShaderType type, const char** sources, unsigned sourceCount)
{
	RgDriverShaderImpl* impl = static_cast<RgDriverShaderImpl*>(self);
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
			rhLog("error", "glCompileShader failed: %s\n", buf.c_str());
		}

		clearError();
		return false;
	}

	checkError();

	return true;
}

typedef struct ProgramUniform
{
	unsigned nameHash;		// Hash value for the uniform name
	GLuint location;
	GLuint arraySize;		// Equals to 1 if not an array
	int texunit;
} ProgramUniform;

typedef struct ProgramAttribute
{
	unsigned nameHash;		// Hash value for the uniform name
	GLuint location;
} ProgramAttribute;

struct RgDriverShaderProgramImpl : public RgDriverShaderProgram
{
	RgDriverShaderProgramImpl() : glh(0) {}
	GLuint glh;
	PreAllocVector<ProgramUniform, 16> uniforms;
	PreAllocVector<ProgramAttribute, 16> attributes;
};	// RgDriverShaderProgramImpl

static ProgramUniform* _findProgramUniform(RgDriverShaderProgram* self, unsigned nameHash)
{
	RgDriverShaderProgramImpl* impl = static_cast<RgDriverShaderProgramImpl*>(self);
	if(!impl) return NULL;

	for(unsigned i=0; i<impl->uniforms.size(); ++i) {
		if(impl->uniforms[i].nameHash == nameHash)
			return &impl->uniforms[i];
	}
	return NULL;
}

static ProgramAttribute* _findProgramAttribute(RgDriverShaderProgram* self, unsigned nameHash)
{
	RgDriverShaderProgramImpl* impl = static_cast<RgDriverShaderProgramImpl*>(self);
	if(!impl) return NULL;

	for(unsigned i=0; i<impl->attributes.size(); ++i) {
		if(impl->attributes[i].nameHash == nameHash)
			return &impl->attributes[i];
	}
	return NULL;
}

static RgDriverShaderProgram* _newShaderProgram()
{
	RgDriverShaderProgramImpl* ret = new RgDriverShaderProgramImpl;
	return ret;
}

static void _deleteShaderProgram(RgDriverShaderProgram* self)
{
	RgDriverShaderProgramImpl* impl = static_cast<RgDriverShaderProgramImpl*>(self);
	if(!impl) return;

	if(impl->glh) {
		glUseProgram(0);
		glDeleteProgram(impl->glh);
	}

	delete static_cast<RgDriverShaderProgramImpl*>(self);
}

static bool _initShaderProgram(RgDriverShaderProgram* self, RgDriverShader** shaders, unsigned shaderCount)
{
	RgDriverShaderProgramImpl* impl = static_cast<RgDriverShaderProgramImpl*>(self);
	if(!impl || !shaders || shaderCount == 0) return false;

	checkError();

	impl->glh = glCreateProgram();

	// Attach shaders
	for(unsigned i=0; i<shaderCount; ++i) {
		if(shaders[i])
			glAttachShader(impl->glh, static_cast<RgDriverShaderImpl*>(shaders[i])->glh);
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
			rhLog("error", "glLinkProgram failed: %s\n", buf.c_str());
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
			rhLog("verbose", "dynamic memory need for PreAllocVector in %s %d\n", __FILE__, __LINE__);

		impl->uniforms.resize(uniformCount);

		for(GLint i=0; i<uniformCount; ++i)
		{
			char uniformName[64];
			GLsizei uniformNameLength;
			GLint uniformSize;
			GLenum uniformType;
			GLuint texunit = 0;

			// See: http://www.opengl.org/sdk/docs/man/xhtml/glGetActiveUniform.xml
			glGetActiveUniform(impl->glh, i, sizeof(uniformName), &uniformNameLength, &uniformSize, &uniformType, uniformName);

			// For array uniform, some driver may return var_name[0]
			for(unsigned j=0; j<sizeof(uniformName); ++j)
				if(uniformName[j] == '[')
					uniformName[j] = '\0';

			ProgramUniform* uniform = &impl->uniforms[i];
			uniform->nameHash = StringHash(uniformName);
			uniform->location = glGetUniformLocation(impl->glh, uniformName);
			uniform->arraySize = uniformSize;

			switch(uniformType) {
				case GL_SAMPLER_2D:
				case GL_SAMPLER_CUBE:
#if !defined(CR_GLES_2)
				case GL_SAMPLER_1D:
				case GL_SAMPLER_3D:
				case GL_SAMPLER_1D_SHADOW:
				case GL_SAMPLER_2D_SHADOW:
#endif
					glUniform1i(i, texunit++);	// Create mapping between uniform location and texture unit
					break;
				default:
					uniform->texunit = -1;
					break;
			}
		}
	}

	checkError();

	{	// Query shader attributes
		GLint attributeCount;
		glGetProgramiv(impl->glh, GL_ACTIVE_ATTRIBUTES, &attributeCount);

		if(impl->attributes.capacity() < (unsigned)attributeCount)
			rhLog("verbose", "dynamic memory need for PreAllocVector in %s %d\n", __FILE__, __LINE__);

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
			attribute->nameHash = StringHash(attributeName);
			attribute->location = glGetAttribLocation(impl->glh, attributeName);
		}
	}

	checkError();

	return true;
}

bool _setUniform1fv(RgDriverShaderProgram* self, unsigned nameHash, const float* value, unsigned count)
{
	if(ProgramUniform* uniform = _findProgramUniform(self, nameHash)) {
		glUniform1fv(uniform->location, count, value);
		return true;
	}
	return false;
}

bool _setUniform2fv(RgDriverShaderProgram* self, unsigned nameHash, const float* value, unsigned count)
{
	if(ProgramUniform* uniform = _findProgramUniform(self, nameHash)) {
		glUniform2fv(uniform->location, count, value);
		return true;
	}
	return false;
}

bool _setUniform3fv(RgDriverShaderProgram* self, unsigned nameHash, const float* value, unsigned count)
{
	if(ProgramUniform* uniform = _findProgramUniform(self, nameHash)) {
		glUniform3fv(uniform->location, count, value);
		return true;
	}
	return false;
}

bool _setUniform4fv(RgDriverShaderProgram* self, unsigned nameHash, const float* value, unsigned count)
{
	if(ProgramUniform* uniform = _findProgramUniform(self, nameHash)) {
		glUniform4fv(uniform->location, count, value);
		return true;
	}
	return false;
}

bool _setUniformMat44fv(RgDriverShaderProgram* self, unsigned nameHash, bool transpose, const float* value, unsigned count)
{
	if(ProgramUniform* uniform = _findProgramUniform(self, nameHash)) {
		glUniformMatrix4fv(uniform->location, count, transpose, value);
		return true;
	}
	return false;
}

bool _setUniformTexture(RgDriverShaderProgram* self, unsigned nameHash, RgDriverTexture* texture)
{
	ProgramUniform* uniform = _findProgramUniform(self, nameHash);
	if(!uniform ) return false;

	checkError();

	if(texture) {
		glActiveTexture(GL_TEXTURE0 + uniform->texunit);
		GLenum glTarget = reinterpret_cast<RgDriverTextureImpl*>(texture)->glTarget;
		GLuint glh = reinterpret_cast<RgDriverTextureImpl*>(texture)->glh;

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

typedef struct ProgramInputMapping
{
	GLenum elementType;
	GLboolean normalized;
} ProgramInputMapping;

ProgramInputMapping _programInputMappings[] = {
	{ 0,		false },
	{ GL_FLOAT,	false },
	{ GL_FLOAT,	false },
	{ GL_FLOAT,	false },
	{ GL_FLOAT,	false },
};

bool _bindProgramInput(RgDriverShaderProgram* self, RgDriverShaderProgramInput* inputs, unsigned inputCount, unsigned* cacheId)
{
//	glShadeModel(GL_SMOOTH);
//	glFrontFace(GL_CCW);			// OpenGl use counterclockwise as the default winding
//	glDepthRange(-1, 1);
//	glDisable(GL_DEPTH_TEST);

	RgDriverShaderProgramImpl* impl = static_cast<RgDriverShaderProgramImpl*>(self);
	if(!impl) return false;

	checkError();

	for(unsigned attri=0; attri<inputCount; ++attri)
	{
		RgDriverShaderProgramInput* i = &inputs[attri];

		if(!i || !i->buffer)
			continue;

		RgDriverBufferImpl* buffer = reinterpret_cast<RgDriverBufferImpl*>(i->buffer);

		// Bind index buffer
		if(buffer->type & RgDriverBufferType_Index)
		{
			if(buffer->type & RgDriverBufferType_System) {
//				crGpuFixedIndexPtr = (char*)buffer->sysMem;
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
			}
			else {
//				crGpuFixedIndexPtr = nullptr;
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer->glh);
			}
		}
		// Bind vertex buffer
		else if(buffer->type & RgDriverBufferType_Vertex)
		{
			ProgramInputMapping* m = &_programInputMappings[i->elementCount];

			// Generate nameHash if necessary
			if(i->nameHash == 0 && i->name)
				i->nameHash = StringHash(i->name);

			// See: http://www.opengl.org/sdk/docs/man/xhtml/glVertexAttribPointer.xml
			if(ProgramAttribute* a = _findProgramAttribute(self, i->nameHash)) {
				if(buffer->type & RgDriverBufferType_System) {
					glBindBuffer(GL_ARRAY_BUFFER, 0);
					glVertexAttribPointer(a->location, i->elementCount, m->elementType, m->normalized, i->stride, (char*)buffer->systemBuf + i->offset);
					glEnableVertexAttribArray(a->location);
				}
				else {
					glBindBuffer(GL_ARRAY_BUFFER, buffer->glh);
					glVertexAttribPointer(a->location, i->elementCount, m->elementType, m->normalized, i->stride, (void*)i->offset);
					glEnableVertexAttribArray(a->location);
				}
			}
			else
				rhLog("error", "attribute not found!\n");
		}
	}

	checkError();

	return true;
}

bool _bindProgramInputCached(RgDriverShaderProgram* self, unsigned cacheId)
{
	RgDriverShaderProgramImpl* impl = static_cast<RgDriverShaderProgramImpl*>(self);
	if(!impl) return false;

	// TODO: Implement
	return false;
}

//////////////////////////////////////////////////////////////////////////
// Making draw call

static void _drawTriangle(unsigned offset, unsigned vertexCount, unsigned flags)
{
	GLenum mode = GL_TRIANGLES;
	glDrawArrays(mode, offset, vertexCount);
}

static void _drawTriangleIndexed(unsigned offset, unsigned indexCount, unsigned flags)
{
	GLenum mode = GL_TRIANGLES;
	GLenum indexType = GL_UNSIGNED_SHORT;
	unsigned byteOffset = offset * sizeof(rhuint16);

	checkError();
	glDrawElements(mode, indexCount, indexType, (void*)byteOffset);
	checkError();
}

//////////////////////////////////////////////////////////////////////////
// Driver

struct RgDriverImpl : public RgDriver
{
};	// RgDriver

static void _rhDeleteRenderDriver_GL(RgDriver* self)
{
	delete static_cast<RgDriverImpl*>(self);
}

}	// namespace

RgDriver* _rhNewRenderDriver_GL(const char* options)
{
	RgDriverImpl* ret = new RgDriverImpl;
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

	ret->newShaderPprogram = _newShaderProgram;
	ret->deleteShaderProgram = _deleteShaderProgram;
	ret->initShader = _initShader;
	ret->initShaderProgram = _initShaderProgram;
	ret->deleteShaderProgram = _deleteShaderProgram;
	ret->initShaderProgram = _initShaderProgram;
	ret->setUniform1fv = _setUniform1fv;
	ret->setUniform2fv = _setUniform2fv;
	ret->setUniform3fv = _setUniform3fv;
	ret->setUniform4fv = _setUniform4fv;
	ret->setUniformMat44fv = _setUniformMat44fv;
	ret->setUniformTexture = _setUniformTexture;
	ret->bindProgramInput = _bindProgramInput;
	ret->bindProgramInputCached = _bindProgramInputCached;

	ret->drawTriangle = _drawTriangle;
	ret->drawTriangleIndexed = _drawTriangleIndexed;

	return ret;
}
