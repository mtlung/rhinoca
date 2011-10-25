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
} ProgramAttribute;

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

	PreAllocVector<RgDriverShaderProgramImpl, 256> shaderProgramCache;
	RgDriverShaderProgramImpl* currentShaderProgram;

	struct TextureState {
		void* hash;
		GLuint glh;
	};
	Array<TextureState, 64> textureStateCache;
};	// RgDriverContextImpl
