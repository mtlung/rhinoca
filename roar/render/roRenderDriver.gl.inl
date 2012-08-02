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

struct roRDriverBufferImpl : public roRDriverBuffer
{
	GLuint glh;
	void* systemBuf;
};	// roRDriverBufferImpl

struct VertexArrayObject
{
	unsigned hash;
	GLuint glh;
	float lastUsedTime;
};

typedef struct TextureFormatMapping
{
	roRDriverTextureFormat format;
	unsigned glPixelSize;
	GLint glInternalFormat;	// eg. GL_RGBA8
	GLenum glFormat;		// eg. GL_RGBA
	GLenum glType;			// eg. GL_UNSIGNED_BYTE
} TextureFormatMapping;

struct roRDriverTextureImpl : public roRDriverTexture
{
	GLuint glh;
	GLenum glTarget;	// eg. GL_TEXTURE_2D
	TextureFormatMapping* formatMapping;

	struct MapInfo {
		GLuint glPbo;
		char* systemBuf;
		roByte* mappedPtr;	// It indicate if the mapping operation is really success or not
		roRDriverMapUsage usage;
	};

	TinyArray<MapInfo, 1> mapInfo;	// For mapping different mip-level and texture array

	friend void roOnMemMove(roRDriverTextureImpl& self, void* newMemLocation) {
		::roOnMemMove(self.mapInfo, &self.mapInfo);
	}
};	// roRDriverTextureImpl

struct roRDriverShaderProgramImpl : public roRDriverShaderProgram
{
	roRDriverShaderProgramImpl() : hash(0), glh(0) {}
	unsigned hash;	/// Base on the shader pointers
	GLuint glh;
	unsigned textureCount;
	Array<roRDriverShaderImpl*> shaders;	/// When ever a shader is destroyed, one should also remove it from this list
	Array<ProgramUniform> uniforms;
	Array<ProgramUniformBlock> uniformBlocks;
	Array<ProgramAttribute> attributes;
};

struct RenderTarget
{
	unsigned hash;
	GLuint glh;
	float lastUsedTime;
};

struct DepthStencilBuffer
{
	GLuint depthHandle;
	GLuint stencilHandle;
	GLuint depthStencilHandle;
	unsigned width;
	unsigned height;
	unsigned refCount;
	float lastUsedTime;
};

struct roRDriverContextImpl : public roRDriverContext
{
	void* currentBlendStateHash;
	void* currentRasterizerStateHash;
	void* currentDepthStencilStateHash;

	void* currentIndexBufSysMemPtr;

	Array<roRDriverShaderProgramImpl> shaderProgramCache;
	roRDriverShaderProgramImpl* currentShaderProgram;

	// A ring of pixel buffer object
	Array<GLuint> pixelBufferInUse;
	Array<GLuint> pixelBufferCache;
	roSize currentPixelBufferIndex;

	Array<roRDriverBufferImpl*> bufferCache;	// We would like to minimize the create/delete of buffer objects

	Array<VertexArrayObject> vaoCache;

	struct TextureState {
		void* hash;
		GLuint glh;
	};
	StaticArray<TextureState, 64> textureStateCache;

	roRDriverCompareFunc currentDepthFunc;
	roRDriverColorWriteMask currentColorWriteMask;

	Array<RenderTarget> renderTargetCache;
	Array<DepthStencilBuffer> depthStencilBufferCache;

	roSize bindedIndexCount;	// For debug purpose
};
