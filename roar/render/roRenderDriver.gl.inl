struct roRDriverShaderImpl : public roRDriverShader
{
	GLuint glh;
};

struct roRDriverShaderProgram
{
};

struct ProgramUniform
{
	unsigned nameHash;		// Hash value for the uniform name
	GLuint location;
	GLuint arraySize;		// Equals to 1 if not an array
	GLenum type;			// GL_FLOAT, GL_FLOAT_VEC4, GL_SAMPLER_2D etc
	int texunit;
};

struct ProgramUniformBlock
{
	unsigned nameHash;		// Hash value for the uniform name
	GLuint index;
	GLint blockSizeInBytes;
};

struct ProgramAttribute
{
	unsigned nameHash;		// Hash value for the uniform name
	GLuint location;
	GLenum type;			// GL_FLOAT_VEC4, GL_FLOAT_MAT4 etc (all are float)
	GLint arraySize;		// Equals to 1 if not an array
	GLenum elementType;		// GL_FLOAT, GL_INT etc
	GLint elementCount;		// 1, 2, 3 or 4
};

struct VertexArrayObject
{
	unsigned hash;
	GLuint glh;
	float hotness;
};

struct roRDriverShaderProgramImpl : public roRDriverShaderProgram
{
	roRDriverShaderProgramImpl() : hash(0), glh(0) {}
	unsigned hash;	/// Base on the shader pointers
	GLuint glh;
	unsigned textureCount;
	TinyArray<roRDriverShaderImpl*, 8> shaders;	/// When ever a shader is destroyed, one should also remove it from this list
	TinyArray<ProgramUniform, 8> uniforms;
	TinyArray<ProgramUniformBlock, 8> uniformBlocks;
	TinyArray<ProgramAttribute, 8> attributes;
};

struct roRDriverContextImpl : public roRDriverContext
{
	void* currentBlendStateHash;
	void* currentDepthStencilStateHash;

	void* currentIndexBufSysMemPtr;

	TinyArray<roRDriverShaderProgramImpl, 64> shaderProgramCache;
	roRDriverShaderProgramImpl* currentShaderProgram;

	// A ring of pixel buffer object
	TinyArray<GLuint, 1> pixelBufferCache;
	unsigned currentPixelBufferIndex;

	TinyArray<VertexArrayObject, 16> vaoCache;

	struct TextureState {
		void* hash;
		GLuint glh;
	};
	StaticArray<TextureState, 64> textureStateCache;
};
