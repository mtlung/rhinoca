#include "pch.h"
#include "driver2.h"

#include "../array.h"
#include "../common.h"
#include "../rhlog.h"
#include "../vector.h"
#include "../rhstring.h"

// DirectX stuffs
// State Object:	http://msdn.microsoft.com/en-us/library/bb205071.aspx
// DX migration:	http://msdn.microsoft.com/en-us/library/windows/desktop/ff476190%28v=vs.85%29.aspx

//////////////////////////////////////////////////////////////////////////
// Common stuffs

//////////////////////////////////////////////////////////////////////////
// Context management

struct RgDriverContextImpl : public RgDriverContext
{
	unsigned currentBlendStateHash;
	unsigned currentDepthStencilStateHash;

	struct TextureState {
		unsigned hash;
	};
	Array<TextureState, 64> textureStateCache;
};	// RgDriverContextImpl

// These functions are implemented in platform specific src files, eg. driver2.gl.windows.cpp
extern RgDriverContext* _newDriverContext_DX11();
extern void _deleteDriverContext_DX11(RgDriverContext* self);
extern bool _initDriverContext_DX11(RgDriverContext* self, void* platformSpecificWindow);
extern void _useDriverContext_DX11(RgDriverContext* self);
extern RgDriverContext* _getCurrentContext_DX11();


//////////////////////////////////////////////////////////////////////////
// Driver

struct RgDriverImpl : public RgDriver
{
};	// RgDriver

static void _rhDeleteRenderDriver_DX11(RgDriver* self)
{
	delete static_cast<RgDriverImpl*>(self);
}

RgDriver* _rhNewRenderDriver_DX11(const char* options)
{
	RgDriverImpl* ret = new RgDriverImpl;
	memset(ret, 0, sizeof(*ret));
	ret->destructor = &_rhDeleteRenderDriver_DX11;

	// Setup the function pointers
	ret->newContext = _newDriverContext_DX11;
	ret->deleteContext = _deleteDriverContext_DX11;
	ret->initContext = _initDriverContext_DX11;
	ret->useContext = _useDriverContext_DX11;
	ret->currentContext = _getCurrentContext_DX11;

	return ret;
}
