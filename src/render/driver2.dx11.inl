struct RgDriverShaderImpl : public RgDriverShader
{
	ID3D11DeviceChild* dxShader;
	ID3D10Blob* dxShaderBlob;
};	// RgDriverShaderImpl

struct RgDriverContextImpl : public RgDriverContext
{
	void* currentBlendStateHash;
	void* currentDepthStencilStateHash;

	ID3D11Device* dxDevice;
	ID3D11DeviceContext* dxDeviceContext;
	IDXGISwapChain* dxSwapChain;
	ID3D11RenderTargetView* dxRenderTargetView;
	ID3D11Texture2D* dxDepthStencilTexture;
	ID3D11DepthStencilView* dxDepthStencilView;

	Array<RgDriverShaderImpl*, 3> currentShaders;

//	Vector<ID3D11DepthStencilState*> depthStencilStateCache;

	struct TextureState {
		void* hash;
	};
	Array<TextureState, 64> textureStateCache;
};	// RgDriverContextImpl
