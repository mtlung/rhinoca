struct RgDriverContextImpl : public RgDriverContext
{
	unsigned currentBlendStateHash;
	unsigned currentDepthStencilStateHash;

	ID3D11Device* dxDevice;
	ID3D11DeviceContext* dxDeviceContext;
	IDXGISwapChain* dxSwapChain;
	ID3D11RenderTargetView* dxRenderTargetView;
	ID3D11Texture2D* dxDepthStencilTexture;
	ID3D11DepthStencilView* dxDepthStencilView;

	struct TextureState {
		unsigned hash;
	};
	Array<TextureState, 64> textureStateCache;
};	// RgDriverContextImpl