struct RgDriverShaderImpl : public RgDriverShader
{
	GLuint glh;
};	// RgDriverShaderImpl

typedef struct RgDriverShaderProgram
{
} RgDriverShaderProgram;

typedef struct ProgramUniform
{
	unsigned nameHash;		// Hash value for the uniform name
	GLuint location;
	GLuint arraySize;		// Equals to 1 if not an array
	GLenum type;			// GL_FLOAT, GL_FLOAT_VEC4, GL_SAMPLER_2D etc
	int texunit;
} ProgramUniform;

typedef struct ProgramUniformBlock
{
	unsigned nameHash;		// Hash value for the uniform name
	GLuint index;
	GLint blockSizeInBytes;
} ProgramUniformBlock;

typedef struct ProgramAttribute
{
	unsigned nameHash;		// Hash value for the uniform name
	GLuint location;
	GLenum type;			// GL_FLOAT_VEC4, GL_FLOAT_MAT4 etc (all are float)
	GLint arraySize;		// Equals to 1 if not an array
	GLenum elementType;		// GL_FLOAT, GL_INT etc
	GLint elementCount;		// 1, 2, 3 or 4
} ProgramAttribute;

typedef struct VertexArrayObject	// TODO: Use a decay mechanizm to clean this up
{
	unsigned hash;
	GLuint glh;
	float hotness;
} VertexArrayObject;

struct RgDriverShaderProgramImpl : public RgDriverShaderProgram
{
	RgDriverShaderProgramImpl() : hash(0), glh(0) {}
	unsigned hash;	/// Base on the shader pointers
	GLuint glh;
	PreAllocVector<RgDriverShaderImpl*, 8> shaders;	/// When ever a shader is destroyed, one should also remove it from this list
	PreAllocVector<ProgramUniform, 8> uniforms;
	PreAllocVector<ProgramUniformBlock, 8> uniformBlocks;
	PreAllocVector<ProgramAttribute, 8> attributes;
};	// RgDriverShaderProgramImpl

struct RgDriverContextImpl : public RgDriverContext
{
	void* currentBlendStateHash;
	void* currentDepthStencilStateHash;

	void* currentIndexBufSysMemPtr;

	PreAllocVector<RgDriverShaderProgramImpl, 64> shaderProgramCache;
	RgDriverShaderProgramImpl* currentShaderProgram;

	// A ring of pixel buffer object
	PreAllocVector<GLuint, 1> pixelBufferCache;
	unsigned currentPixelBufferIndex;

	PreAllocVector<VertexArrayObject, 16> vaoCache;

	struct TextureState {
		void* hash;
		GLuint glh;
	};
	Array<TextureState, 64> textureStateCache;
};	// RgDriverContextImpl
