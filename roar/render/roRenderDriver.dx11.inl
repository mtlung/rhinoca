// Ref: http://www.flipcode.com/archives/COM_Smart_Pointer.shtml
// NOTE: For this implementation, we won't take ownership from raw pointer,
// Since most object created by DX has already increment the ref count.
template<typename T> struct ComPtr
{
	ComPtr() : ptr(NULL) {}
	explicit ComPtr(T* p) : ptr(p) {}	// NOTE: No AddRef for raw pointer
	ComPtr(const ComPtr& rhs) : ptr(rhs.ptr) { if(ptr) ptr->AddRef(); }
	~ComPtr() { if(ptr) ptr->Release(); }
	ComPtr& operator=(const ComPtr& rhs) { ComPtr tmp(rhs); swap(tmp); return *this; }
	template<typename U>
	ComPtr& operator=(U* rhs) { ComPtr tmp(static_cast<T*>(rhs)); swap(tmp); return *this; }
	void swap(ComPtr& rhs) { T* tmp = ptr; ptr = rhs.ptr; rhs.ptr = tmp; }
	T* operator->() const { return ptr; }
	operator T*() const { return ptr; }
	T* ptr;
};

struct InputParam
{
	StringHash nameHash;		// Hash value for the input semantic
	roUint32 elementCount;
	roInt32 type;	// D3D10_REGISTER_COMPONENT_TYPE
};

struct ShaderResourceBinding
{
	StringHash nameHash;		// Hash value for the shader resource
	roUint32 bindPoint;
};

struct StagingBuffer
{
	StagingBuffer() : size(0), mapped(false), lastUsedTime(0) {}
	unsigned size:31;
	unsigned mapped:1;
	float lastUsedTime;
	ComPtr<ID3D11Buffer> dxBuffer;
};

struct StagingTexture
{
	StagingTexture() : hash(0), mapped(false), lastUsedTime(0) {}
	unsigned hash;
	unsigned mapped;
	float lastUsedTime;
	ComPtr<ID3D11Resource> dxTexture;
};

struct SamplerState {
	void* hash;
	ComPtr<ID3D11SamplerState> dxSamplerState;
};

struct BlendState {
	void* hash;
	float lastUsedTime;
	ComPtr<ID3D11BlendState> dxBlendState;
};

struct RasterizerState {
	void* hash;
	float lastUsedTime;
	ComPtr<ID3D11RasterizerState> dxRasterizerState;
};

struct DepthStencilState {
	void* hash;
	float lastUsedTime;
	ComPtr<ID3D11DepthStencilState> dxDepthStencilState;
};

struct BufferCacheEntry {
	void* hash;	// Capture D3D11_BUFFER_DESC without the ByteWidth
	roSize sizeInByte;
	float lastUsedTime;
	ComPtr<ID3D11Buffer> dxBuffer;
};

struct roRDriverBufferImpl : public roRDriverBuffer
{
	void* hash;
	roSize capacity;	// This is the actual size of the dxBuffer
	ComPtr<ID3D11Buffer> dxBuffer;
	int dxStagingIdx;	// Index into stagingBufferCache
};

struct roRDriverTextureImpl : public roRDriverTexture
{
	ComPtr<ID3D11Resource> dxTexture;	// May store a 1d, 2d or 3d texture
	ComPtr<ID3D11ShaderResourceView> dxView;
	D3D11_RESOURCE_DIMENSION dxDimension;
	int dxStagingIdx;	// Index into stagingTextureCache
};

struct roRDriverShaderImpl : public roRDriverShader
{
	roRDriverShaderImpl() : dxShader(NULL) {}

	ComPtr<ID3D11DeviceChild> dxShader;

	Array<roByte> shaderBlob;
	Array<InputParam> inputParams;
	Array<ShaderResourceBinding> shaderResourceBindings;
};

struct InputLayout
{
	InputLayout() : hash(0), lastUsedTime(0), shaderBlob(NULL) {}
	unsigned hash;
	float lastUsedTime;

	Array<roByte>* shaderBlob;

	ComPtr<ID3D11InputLayout> layout;
	Array<D3D11_INPUT_ELEMENT_DESC> inputDescs;

	Array<UINT> strides;
	Array<UINT> offsets;
};

struct RenderTarget
{
	unsigned hash;
	float lastUsedTime;
	Array<ComPtr<ID3D11RenderTargetView> > rtViews;
	ComPtr<ID3D11DepthStencilView> depthStencilView;

	friend void roOnMemMove(RenderTarget& self, void* newMemLocation) {
		::roOnMemMove(self.rtViews, &self.rtViews);
	}
};

struct roRDriverContextImpl : public roRDriverContext
{
	void* currentBlendStateHash;
	void* currentRasterizerStateHash;
	void* currentDepthStencilStateHash;

	ComPtr<ID3D11Device> dxDevice;
	ComPtr<ID3D11DeviceContext> dxDeviceContext;
	ComPtr<IDXGISwapChain> dxSwapChain;
	ComPtr<ID3D11RenderTargetView> dxRenderTargetView;
	ComPtr<ID3D11Texture2D> dxDepthStencilTexture;
	ComPtr<ID3D11DepthStencilView> dxDepthStencilView;

	typedef StaticArray<roRDriverShaderImpl*, 3> CurrentShaders;
	CurrentShaders currentShaders;

	Array<InputLayout> inputLayoutCache;
	roSize stagingBufferCacheSearchIndex;
	Array<StagingBuffer> stagingBufferCache;
	roSize stagingTextureCacheSearchIndex;
	Array<StagingTexture> stagingTextureCache;

	Array<BlendState> blendStateCache;
	Array<RasterizerState> rasterizerState;
	Array<DepthStencilState> depthStencilStateCache;
	StaticArray<SamplerState, 64> samplerStateCache;

	Array<RenderTarget> renderTargetCache;
	unsigned currentRenderTargetViewHash;

	Array<BufferCacheEntry> bufferCache;
	StaticArray<roRDriverBufferImpl*, 3> constBufferInUse;

	roSize triangleFanIndexBufferSize;
	roRDriverBuffer* triangleFanIndexBuffer;	// An index buffer dedicated to draw triangle fan

	roSize bindedIndexCount;	// For debug purpose
};
