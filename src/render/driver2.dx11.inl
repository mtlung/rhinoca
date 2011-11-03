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

typedef struct ConstantBuffer
{
	unsigned nameHash;		// Hash value for the constant buffer
	unsigned bindPoint;
} ConstantBuffer;

struct RgDriverShaderImpl : public RgDriverShader
{
	RgDriverShaderImpl() : dxShader(NULL), dxShaderBlob(NULL) {}

	ComPtr<ID3D11DeviceChild> dxShader;
	ComPtr<ID3D10Blob> dxShaderBlob;

	PreAllocVector<ConstantBuffer, 4> constantBuffers;
};	// RgDriverShaderImpl

struct RgDriverContextImpl : public RgDriverContext
{
	void* currentBlendStateHash;
	void* currentDepthStencilStateHash;

	ComPtr<ID3D11Device> dxDevice;
	ComPtr<ID3D11DeviceContext> dxDeviceContext;
	ComPtr<IDXGISwapChain> dxSwapChain;
	ComPtr<ID3D11RenderTargetView> dxRenderTargetView;
	ComPtr<ID3D11Texture2D> dxDepthStencilTexture;
	ComPtr<ID3D11DepthStencilView> dxDepthStencilView;

	Array<RgDriverShaderImpl*, 3> currentShaders;

//	Vector<ID3D11DepthStencilState*> depthStencilStateCache;

	struct TextureState {
		void* hash;
	};
	Array<TextureState, 64> textureStateCache;
};	// RgDriverContextImpl
