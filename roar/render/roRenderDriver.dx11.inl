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
	operator bool() const { return ptr != NULL; }
	operator T*() const { return ptr; }
	T* ptr;
};

struct InputParam
{
	StringHash nameHash;		// Hash value for the input semantic
	unsigned elementCount;
	D3D10_REGISTER_COMPONENT_TYPE type;
};

struct ConstantBuffer
{
	StringHash nameHash;		// Hash value for the constant buffer
	unsigned bindPoint;
};

struct StagingBuffer
{
	unsigned size:29;
	unsigned mapped:1;
	unsigned busyFrame:2;	// How many frame till the staging buffer becomes available again
	float hotness;
	ComPtr<ID3D11Buffer> dxBuffer;
};

struct StagingTexture
{
	unsigned hash;
	float hotness;
	ComPtr<ID3D11Resource> dxTexture;
};

struct roRDriverShaderImpl : public roRDriverShader
{
	roRDriverShaderImpl() : dxShader(NULL), dxShaderBlob(NULL) {}

	ComPtr<ID3D11DeviceChild> dxShader;
	ComPtr<ID3D10Blob> dxShaderBlob;

	TinyArray<InputParam, 4> inputParams;
	TinyArray<ConstantBuffer, 4> constantBuffers;
};	// roRDriverShaderImpl

struct InputLayout
{
	unsigned hash;
	float hotness;

	ComPtr<ID3D11InputLayout> layout;
	ComPtr<ID3D10Blob> shader;
	TinyArray<D3D11_INPUT_ELEMENT_DESC, 4> inputDescs;

	TinyArray<ComPtr<ID3D11Buffer>, 4> buffers;
	TinyArray<UINT, 4> strides;
	TinyArray<UINT, 4> offsets;
};

struct roRDriverContextImpl : public roRDriverContext
{
	void* currentBlendStateHash;
	void* currentDepthStencilStateHash;

	ComPtr<ID3D11Device> dxDevice;
	ComPtr<ID3D11DeviceContext> dxDeviceContext;
	ComPtr<IDXGISwapChain> dxSwapChain;
	ComPtr<ID3D11RenderTargetView> dxRenderTargetView;
	ComPtr<ID3D11Texture2D> dxDepthStencilTexture;
	ComPtr<ID3D11DepthStencilView> dxDepthStencilView;

	typedef StaticArray<roRDriverShaderImpl*, 3> CurrentShaders;
	CurrentShaders currentShaders;

	TinyArray<InputLayout, 16> inputLayoutCache;

	Array<StagingBuffer> stagingBufferCache;
	Array<StagingTexture> stagingTextureCache;

//	Array<ID3D11DepthStencilState*> depthStencilStateCache;

	struct TextureState {
		void* hash;
	};
	StaticArray<TextureState, 64> textureStateCache;
};	// roRDriverContextImpl