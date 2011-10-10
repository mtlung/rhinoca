#include "pch.h"
#include "driver2.h"

#include <gl/gl.h>
#include "gl/glext.h"
#include "win32/extensionswin32fwd.h"

#include "../array.h"
#include "../common.h"
#include "../vector.h"
#include "../rhstring.h"

// DirectX stuffs
// State Object:	http://msdn.microsoft.com/en-us/library/bb205071.aspx
// DX migration:	http://msdn.microsoft.com/en-us/library/windows/desktop/ff476190%28v=vs.85%29.aspx

//////////////////////////////////////////////////////////////////////////
// Common stuffs

#define checkError() { GLenum err = glGetError(); if(err != GL_NO_ERROR) print(NULL, "unhandled GL error 0x%04x before %s %d\n", err, __FILE__, __LINE__); }
#define clearError() { glGetError(); }

//////////////////////////////////////////////////////////////////////////
// Context management

// These functions are implemented in platform specific src files, eg. driver2.gl.windows.cpp
extern RhRenderDriverContext* _newDriverContext();
extern void _useDriverContext(RhRenderDriverContext* self);
extern void _deleteDriverContext(RhRenderDriverContext* self);
extern bool _initContext(RhRenderDriverContext* self, void* platformSpecificWindow);
extern void _swapBuffers(RhRenderDriverContext* self);
extern bool _changeResolution(RhRenderDriverContext* self, unsigned width, unsigned height);

static void _setViewport(RhRenderDriverContext* self, unsigned x, unsigned y, unsigned width, unsigned height)
{
//	glEnable(GL_SCISSOR_TEST);
	glViewport((GLint)x, (GLint)y, (GLsizei)width, (GLsizei)height);
//	glScissor((GLint)x, (GLint)y, (GLsizei)width, (GLsizei)height);
//	glDepthRange(zmin, zmax);
}

//////////////////////////////////////////////////////////////////////////
// Buffer

static Array<GLenum, 9> _bufferTarget = {
	0,
	GL_ARRAY_BUFFER,
	GL_ELEMENT_ARRAY_BUFFER, 0,
#if !defined(CR_GLES_2)
	GL_UNIFORM_BUFFER, 0, 0, 0, 0
#endif
};

#if !defined(CR_GLES_2)
static Array<GLenum, 4> _bufferMapUsage = {
	0,
	GL_READ_ONLY,
	GL_WRITE_ONLY,
	GL_READ_WRITE,
};
#endif

struct RhRenderBufferImpl : public RhRenderBuffer
{
	GLuint glh;
	void* systemBuf;
};	// RhRenderBufferImpl

static RhRenderBuffer* _newBuffer()
{
	RhRenderBufferImpl* ret = new RhRenderBufferImpl;
	memset(ret, 0, sizeof(*ret));
	return ret;
}

static void _deleteBuffer(RhRenderBuffer* self)
{
	RhRenderBufferImpl* impl = static_cast<RhRenderBufferImpl*>(self);
	if(!impl) return;

	if(impl->glh)
		glDeleteBuffers(1, &impl->glh);

	rhinoca_free(impl->systemBuf);
	delete static_cast<RhRenderBufferImpl*>(self);
}

static bool _initBuffer(RhRenderBuffer* self, RhRenderBufferType type, void* initData, unsigned sizeInBytes)
{
	RhRenderBufferImpl* impl = static_cast<RhRenderBufferImpl*>(self);
	if(!impl) return false;

	self->type = type;
	self->sizeInBytes = sizeInBytes;

	if(type & RhRenderBufferType_System) {
		impl->systemBuf = rhinoca_malloc(sizeInBytes);
		if(initData)
			memcpy(impl->systemBuf, initData, sizeInBytes);
	}
	else {
		checkError();
		glGenBuffers(1, &impl->glh);
		RHASSERT("Invalid RhRenderBufferType" && _bufferTarget[type] != 0);
		glBindBuffer(_bufferTarget[type], impl->glh);
		glBufferData(_bufferTarget[type], sizeInBytes, initData, GL_STREAM_DRAW);
		RHASSERT(impl->glh);
		checkError();
	}

	return true;
}

static bool _updateBuffer(RhRenderBuffer* self, unsigned offsetInBytes, void* data, unsigned sizeInBytes)
{
	RhRenderBufferImpl* impl = static_cast<RhRenderBufferImpl*>(self);
	if(!impl) return false;

	if(offsetInBytes + sizeInBytes > self->sizeInBytes)
		return false;

	if(self->type & RhRenderBufferType_System)
		memcpy(((char*)impl->systemBuf) + offsetInBytes, data, sizeInBytes);
	else {
		checkError();
		glBindBuffer(_bufferTarget[self->type], impl->glh);
		glBufferSubData(_bufferTarget[self->type], offsetInBytes, sizeInBytes, data);
		checkError();
	}

	return true;
}

static void* _mapBuffer(RhRenderBuffer* self, RhRenderBufferMapUsage usage)
{
	RhRenderBufferImpl* impl = static_cast<RhRenderBufferImpl*>(self);
	if(!impl) return false;

	if(impl->systemBuf)
		return impl->systemBuf;

#if !defined(CR_GLES_2)
	void* ret = NULL;
	checkError();
	RHASSERT("Invalid RhRenderBufferMapUsage" && _bufferMapUsage[usage] != 0);
	glBindBuffer(_bufferTarget[self->type], impl->glh);
	ret = glMapBuffer(_bufferTarget[self->type], _bufferMapUsage[usage]);
	checkError();
	return ret;
#endif

	return NULL;
}

static void _unmapBuffer(RhRenderBuffer* self)
{
	RhRenderBufferImpl* impl = static_cast<RhRenderBufferImpl*>(self);
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
	RhRenderTextureFormat format;
	unsigned glPixelSize;
	GLint glInternalFormat;	// eg. GL_RGBA8
	GLenum glFormat;		// eg. GL_RGBA
	GLenum glType;			// eg. GL_UNSIGNED_BYTE
} TextureFormatMapping;

TextureFormatMapping _textureFormatMappings[] = {
	{ RhRenderTextureFormat_RGBA,			4, GL_RGBA8, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8 },
	{ RhRenderTextureFormat_R,				1, GL_R8, GL_RED, GL_UNSIGNED_BYTE },
	{ RhRenderTextureFormat_A,				1, GL_ALPHA, GL_ALPHA, GL_UNSIGNED_BYTE },
	{ RhRenderTextureFormat_Depth,			0, 0, 0, 0 },
	{ RhRenderTextureFormat_DepthStencil,	0, 0, 0, 0 },
	{ RhRenderTextureFormat_PVRTC2,			0, 0, 0, 0 },
	{ RhRenderTextureFormat_PVRTC4,			0, 0, 0, 0 },
//	{ RhRenderTextureFormat_PVRTC2,			2, GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG, 0, 0 },
//	{ RhRenderTextureFormat_PVRTC4,			1, GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG, 0, 0 },
	{ RhRenderTextureFormat_DXT1,			0, 0, 0, 0 },
	{ RhRenderTextureFormat_DXT5,			0, 0, 0, 0 },
};

struct RhRenderTextureImpl : public RhRenderTexture
{
	GLuint glh;
	GLenum glTarget;	// eg. GL_TEXTURE_2D
	TextureFormatMapping* formatMapping;
};	// RhRenderTextureImpl

RhRenderTexture* _newTexture()
{
	RhRenderTextureImpl* ret = new RhRenderTextureImpl;
	memset(ret, 0, sizeof(*ret));
	return ret;
}

void _deleteTexture(RhRenderTexture* self)
{
	RhRenderTextureImpl* impl = static_cast<RhRenderTextureImpl*>(self);
	if(!impl) return;

	if(impl->glh)
		glDeleteTextures(1, &impl->glh);

	delete static_cast<RhRenderTextureImpl*>(self);
}

bool _initTexture(RhRenderTexture* self, unsigned width, unsigned height, RhRenderTextureFormat format)
{
	RhRenderTextureImpl* impl = static_cast<RhRenderTextureImpl*>(self);
	if(!impl) return false;
	if(impl->format || impl->glh) return false;

	impl->width = width;
	impl->height = height;
	impl->format = format;
	impl->glTarget = GL_TEXTURE_2D;
	impl->formatMapping = &(_textureFormatMappings[format]);

	return true;
}

unsigned _mipLevelOffset(RhRenderTextureImpl* self, unsigned mipIndex, unsigned& mipWidth, unsigned& mipHeight)
{
	unsigned i = 0;
	unsigned offset = 0;

	mipWidth = self->width;
	mipHeight = self->height;

	for(unsigned i=0; i<mipIndex; ++i) {
		unsigned mipSize = mipWidth * mipHeight;

		if(RhRenderTextureFormat_Compressed & self->format)
			mipSize = mipSize >> self->formatMapping->glPixelSize;
		else
			mipSize = mipSize * self->formatMapping->glPixelSize;

		offset += mipSize;
		if(mipWidth > 1) mipWidth /= 2;
		if(mipHeight > 1) mipHeight /= 2;
	} while(++i < mipIndex);

	return offset;
}

unsigned char* _mipLevelData(RhRenderTextureImpl* self, unsigned mipIndex, unsigned& mipWidth, unsigned& mipHeight, void* data)
{
	mipWidth = self->width;
	mipHeight = self->height;

	if(!data)
		return NULL;

	return (unsigned char*)(data) + _mipLevelOffset(self, mipIndex, mipWidth, mipHeight);
}

void _commitTexture(RhRenderTexture* self, void* data)
{
	RhRenderTextureImpl* impl = static_cast<RhRenderTextureImpl*>(self);
	if(!impl) return;
	if(!impl->format) return;

	checkError();

	glGenTextures(1, &impl->glh);
	glBindTexture(impl->glTarget, impl->glh);

	const unsigned mipCount = 1;	// TODO: Allow loading mip maps
	TextureFormatMapping* mapping = impl->formatMapping;

	for(unsigned i=0; i<mipCount; ++i)
	{
		unsigned mipw, miph;
		unsigned char* mipData = _mipLevelData(impl, i, mipw, miph, data);

		if(impl->format & RhRenderTextureFormat_Compressed) {
			unsigned imgSize = (mipw * miph) >> mapping->glPixelSize;
			glCompressedTexImage2D(
				impl->glTarget, i, mapping->glInternalFormat, 
				mipw, miph, 0,
				imgSize,
				mipData
			);
		}
		else {
			glTexImage2D(
				impl->glTarget, i, impl->formatMapping->glInternalFormat,
				mipw, miph, 0,
				mapping->glFormat, mapping->glType,
				mipData
			);
		}
	}

	checkError();
}

//////////////////////////////////////////////////////////////////////////
// Shader

static Array<GLenum, 5> _shaderTypes = {
	GL_VERTEX_SHADER,
	GL_FRAGMENT_SHADER,
#if !defined(CR_GLES_2)
	GL_GEOMETRY_SHADER,
	GL_TESS_CONTROL_SHADER,
	GL_TESS_EVALUATION_SHADER,
#endif
};

struct RhRenderShaderImpl : public RhRenderShader
{
	GLuint glh;
};	// RhRenderShaderImpl

static RhRenderShader* _newShader()
{
	RhRenderShaderImpl* ret = new RhRenderShaderImpl;
	memset(ret, 0, sizeof(*ret));
	return ret;
}

static void _deleteShader(RhRenderShader* self)
{
	RhRenderShaderImpl* impl = static_cast<RhRenderShaderImpl*>(self);
	if(!impl) return;

	if(impl->glh)
		glDeleteShader(impl->glh);

	delete static_cast<RhRenderShaderImpl*>(self);
}

bool _initShader(RhRenderShader* self, RhRenderShaderType type, const char** sources, unsigned sourceCount)
{
	RhRenderShaderImpl* impl = static_cast<RhRenderShaderImpl*>(self);
	if(!impl) return false;

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
			print(NULL, "glCompileShader failed: %s", buf.c_str());
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

struct RhRenderShaderProgramImpl : public RhRenderShaderProgram
{
	RhRenderShaderProgramImpl() : glh(0) {}
	GLuint glh;
	PreAllocVector<ProgramUniform, 16> uniforms;
	PreAllocVector<ProgramAttribute, 16> attributes;
};	// RhRenderShaderProgramImpl

static ProgramUniform* _findProgramUniform(RhRenderShaderProgram* self, unsigned nameHash)
{
	RhRenderShaderProgramImpl* impl = static_cast<RhRenderShaderProgramImpl*>(self);
	if(!impl) return NULL;

	for(unsigned i=0; i<impl->uniforms.size(); ++i) {
		if(impl->uniforms[i].nameHash == nameHash)
			return &impl->uniforms[i];
	}
	return NULL;
}

static ProgramAttribute* _findProgramAttribute(RhRenderShaderProgram* self, unsigned nameHash)
{
	RhRenderShaderProgramImpl* impl = static_cast<RhRenderShaderProgramImpl*>(self);
	if(!impl) return NULL;

	for(unsigned i=0; i<impl->attributes.size(); ++i) {
		if(impl->attributes[i].nameHash == nameHash)
			return &impl->attributes[i];
	}
	return NULL;
}

static RhRenderShaderProgram* _newShaderProgram()
{
	RhRenderShaderProgramImpl* ret = new RhRenderShaderProgramImpl;
	return ret;
}

static void _deleteShaderProgram(RhRenderShaderProgram* self)
{
	RhRenderShaderProgramImpl* impl = static_cast<RhRenderShaderProgramImpl*>(self);
	if(!impl) return;

	if(impl->glh) {
		glUseProgram(0);
		glDeleteProgram(impl->glh);
	}

	delete static_cast<RhRenderShaderProgramImpl*>(self);
}

bool _initShaderProgram(RhRenderShaderProgram* self, RhRenderShader** shaders, unsigned shaderCount)
{
	RhRenderShaderProgramImpl* impl = static_cast<RhRenderShaderProgramImpl*>(self);
	if(!impl) return false;

	checkError();

	impl->glh = glCreateProgram();

	// Attach shaders
	for(unsigned i=0; i<shaderCount; ++i) {
		if(shaders[i])
			glAttachShader(impl->glh, static_cast<RhRenderShaderImpl*>(shaders[i])->glh);
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
			print(NULL, "glLinkProgram failed: %s", buf.c_str());
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
			print(NULL, "dynamic memory need for PreAllocVector in %s %d\n", __FILE__, __LINE__);

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
			print(NULL, "dynamic memory need for PreAllocVector in %s %d\n", __FILE__, __LINE__);

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

bool _setUniform1fv(RhRenderShaderProgram* self, unsigned nameHash, const float* value, unsigned count)
{
	if(ProgramUniform* uniform = _findProgramUniform(self, nameHash)) {
		glUniform1fv(uniform->location, count, value);
		return true;
	}
	return false;
}

bool _setUniform2fv(RhRenderShaderProgram* self, unsigned nameHash, const float* value, unsigned count)
{
	if(ProgramUniform* uniform = _findProgramUniform(self, nameHash)) {
		glUniform2fv(uniform->location, count, value);
		return true;
	}
	return false;
}

bool _setUniform3fv(RhRenderShaderProgram* self, unsigned nameHash, const float* value, unsigned count)
{
	if(ProgramUniform* uniform = _findProgramUniform(self, nameHash)) {
		glUniform3fv(uniform->location, count, value);
		return true;
	}
	return false;
}

bool _setUniform4fv(RhRenderShaderProgram* self, unsigned nameHash, const float* value, unsigned count)
{
	if(ProgramUniform* uniform = _findProgramUniform(self, nameHash)) {
		glUniform4fv(uniform->location, count, value);
		return true;
	}
	return false;
}

bool _setUniformMatrix4fv(RhRenderShaderProgram* self, unsigned nameHash, bool transpose, const float* value, unsigned count)
{
	if(ProgramUniform* uniform = _findProgramUniform(self, nameHash)) {
		glUniformMatrix4fv(uniform->location, count, transpose, value);
		return true;
	}
	return false;
}

bool _setUniformTexture(RhRenderShaderProgram* self, unsigned nameHash, RhRenderTexture* texture)
{
	ProgramUniform* uniform = _findProgramUniform(self, nameHash);
	if(!uniform ) return false;

	checkError();

	if(texture) {
		glActiveTexture(GL_TEXTURE0 + uniform->texunit);
		GLenum glTarget = reinterpret_cast<RhRenderTextureImpl*>(texture)->glTarget;
		GLuint glh = reinterpret_cast<RhRenderTextureImpl*>(texture)->glh;

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

bool _bindProgramInput(RhRenderShaderProgram* self, RhRenderShaderProgramInput* inputs, unsigned inputCount, unsigned* cacheId)
{
//	glShadeModel(GL_SMOOTH);
//	glFrontFace(GL_CCW);			// OpenGl use counterclockwise as the default winding
//	glDepthRange(-1, 1);
//	glDisable(GL_DEPTH_TEST);

	RhRenderShaderProgramImpl* impl = static_cast<RhRenderShaderProgramImpl*>(self);
	if(!impl) return false;

	checkError();

	for(unsigned attri=0; attri<inputCount; ++attri)
	{
		RhRenderShaderProgramInput* i = &inputs[attri];

		if(!i || !i->buffer)
			continue;

		RhRenderBufferImpl* buffer = reinterpret_cast<RhRenderBufferImpl*>(i->buffer);

		// Bind index buffer
		if(buffer->type & RhRenderBufferType_Index)
		{
			if(buffer->type & RhRenderBufferType_System) {
//				crGpuFixedIndexPtr = (char*)buffer->sysMem;
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
			}
			else {
//				crGpuFixedIndexPtr = nullptr;
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer->glh);
			}
		}
		// Bind vertex buffer
		else if(buffer->type & RhRenderBufferType_Vertex)
		{
			ProgramInputMapping* m = &_programInputMappings[i->elementCount];

			// Generate nameHash if necessary
			if(i->nameHash == 0 && i->name)
				i->nameHash = StringHash(i->name);

			// See: http://www.opengl.org/sdk/docs/man/xhtml/glVertexAttribPointer.xml
			if(ProgramAttribute* a = _findProgramAttribute(self, i->nameHash)) {
				if(buffer->type & RhRenderBufferType_System) {
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
				print(NULL, "attribute not found!");
		}
	}

	checkError();

	return true;
}

bool _bindProgramInputCached(RhRenderShaderProgram* self, unsigned cacheId)
{
	RhRenderShaderProgramImpl* impl = static_cast<RhRenderShaderProgramImpl*>(self);
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

struct RhRenderDriverImpl : public RhRenderDriver
{
};	// RhRenderDriver

RhRenderDriver* rhNewRenderDriver(const char* options)
{
	RhRenderDriverImpl* ret = new RhRenderDriverImpl;
	memset(ret, 0, sizeof(*ret));

	// Setup the function pointers
	ret->newContext = _newDriverContext;
	ret->useContext = _useDriverContext;
	ret->deleteContext = _deleteDriverContext;
	ret->initContext = _initContext;
	ret->swapBuffers = _swapBuffers;
	ret->changeResolution = _changeResolution;
	ret->setViewport = _setViewport;

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
	ret->setUniformMatrix4fv = _setUniformMatrix4fv;
	ret->setUniformTexture = _setUniformTexture;
	ret->bindProgramInput = _bindProgramInput;
	ret->bindProgramInputCached = _bindProgramInputCached;

	ret->drawTriangle = _drawTriangle;
	ret->drawTriangleIndexed = _drawTriangleIndexed;

	return ret;
}

void rhDeleteRenderDriver(RhRenderDriver* self)
{
	delete static_cast<RhRenderDriverImpl*>(self);
}
