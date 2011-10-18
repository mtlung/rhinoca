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
	PreAllocVector<ProgramUniform, 16> uniforms;
	PreAllocVector<ProgramAttribute, 16> attributes;
};	// RgDriverShaderProgramImpl

struct RgDriverContextImpl : public RgDriverContext
{
	unsigned currentBlendStateHash;
	unsigned currentDepthStencilStateHash;

	PreAllocVector<RgDriverShaderProgramImpl, 256> shaderProgramCache;
	RgDriverShaderProgramImpl* currentShaderProgram;

	struct TextureState {
		unsigned hash;
		GLuint glh;
	};
	Array<TextureState, 64> textureStateCache;
};	// RgDriverContextImpl
