#include "pch.h"
#include "roCanvas.h"
#include "../base/roStringHash.h"

namespace ro {

Canvas::Canvas()
	: _context(NULL)
	, _vShader(NULL), _pShader(NULL)
{}

Canvas::~Canvas()
{
	destroy();
}

void Canvas::init(roRDriverContext* context)
{
	if(!context) return;
	_context = context;
	_driver = context->driver;

	_vShader = _driver->newShader();
	_pShader = _driver->newShader();

	int driverIndex = 0;
	
	if(roStrCaseCmp(_driver->driverName, "gl") == 0)
		driverIndex = 0;
	if(roStrCaseCmp(_driver->driverName, "dx11") == 0)
		driverIndex = 1;

	static const char* vShaderSrc[] = 
	{
		// GLSL
		"in vec4 position;"
		"in vec2 texCoord;"
		"out varying vec2 _texCoord;"
		"void main(void) {"
		"	position.y = -position.y; texCoord.y = 1-texCoord.y;\n"	// Flip y axis
		"	gl_Position = position;  _texCoord = texCoord;"
		"}",

		// HLSL
		"struct VertexInputType { float4 pos : POSITION; float2 texCoord : TEXCOORD0; };"
		"struct PixelInputType { float4 pos : SV_POSITION; float2 texCoord : TEXCOORD0; };"
		"PixelInputType main(VertexInputType input) {"
		"	PixelInputType output; output.pos = input.pos; output.texCoord = input.texCoord;"
		"	output.pos.y = -output.pos.y; output.texCoord.y = 1-output.texCoord.y;\n"	// Flip y axis
		"	return output;"
		"}"
	};

	static const char* pShaderSrc[] = 
	{
		// GLSL
		"uniform sampler2D tex;"
		"in vec2 _texCoord;"
		"void main(void) { gl_FragColor = texture2D(tex, _texCoord); }",

		// HLSL
		"Texture2D tex;"
		"SamplerState sampleType;"
		"struct PixelInputType { float4 pos : SV_POSITION; float2 texCoord : TEXCOORD0; };"
		"float4 main(PixelInputType input):SV_Target {"
		"	return tex.Sample(sampleType, input.texCoord);"
		"}"
	};

	roVerify(_driver->initShader(_vShader, roRDriverShaderType_Vertex, &vShaderSrc[driverIndex], 1));
	roVerify(_driver->initShader(_pShader, roRDriverShaderType_Pixel, &pShaderSrc[driverIndex], 1));

	// Create vertex buffer
	float vertex[][6] = { {-1,1,0,1, 0,1}, {-1,-1,0,1, 0,0}, {1,-1,0,1, 1,0}, {1,1,0,1, 1,1} };
	_vBuffer = _driver->newBuffer();
	roVerify(_driver->initBuffer(_vBuffer, roRDriverBufferType_Vertex, roRDriverDataUsage_Stream, vertex, sizeof(vertex)));

	// Create index buffer
	roUint16 index[][3] = { {0, 1, 2}, {0, 2, 3} };
	_iBuffer = _driver->newBuffer();
	roVerify(_driver->initBuffer(_iBuffer, roRDriverBufferType_Index, roRDriverDataUsage_Static, index, sizeof(index)));

	// Bind shader input layout
	const roRDriverShaderInput input[] = {
		{ _vBuffer, _vShader, "position", 0, 0, sizeof(float)*6, 0 },
		{ _vBuffer, _vShader, "texCoord", 0, sizeof(float)*4, sizeof(float)*6, 0 },
		{ _iBuffer, NULL, NULL, 0, 0, 0, 0 },
	};
	roAssert(roCountof(input) == _inputLayout.size());
	for(roSize i=0; i<roCountof(input); ++i)
		_inputLayout[i] = input[i];

	// Set the texture state
	roRDriverTextureState textureState =  {
		0,
		roRDriverTextureFilterMode_MinMagLinear,
		roRDriverTextureAddressMode_Repeat, roRDriverTextureAddressMode_Repeat
	};
	_textureState = textureState;
}

bool Canvas::initTargetTexture(unsigned width, unsigned height)
{
	targetTexture = new Texture("");
	targetTexture->handle = _driver->newTexture();
	if(!_driver->initTexture(targetTexture->handle, width, height, roRDriverTextureFormat_RGBA, roRDriverTextureFlag_RenderTarget))
		return false;
	if(!_driver->commitTexture(targetTexture->handle, NULL, 0))
		return false;
	return true;
}

void Canvas::destroy()
{
	if(!_context || !_driver) return;
	_driver->deleteBuffer(_vBuffer);
	_driver->deleteBuffer(_iBuffer);
	_driver->deleteShader(_vShader);
	_driver->deleteShader(_pShader);
	_driver = NULL;
	_context = NULL;
	_vBuffer = _iBuffer = NULL;
	_vShader = _pShader = NULL;
}

void Canvas::beginDraw()
{
	if(!targetTexture || !targetTexture->handle) {
		roVerify(_driver->setRenderTargets(NULL, 0, false));
		_targetWidth = (float)_context->width;
		_targetHeight = (float)_context->height;
	}
	else {
		roVerify(_driver->setRenderTargets(&targetTexture->handle, 1, false));
		_targetWidth = (float)targetTexture->handle->width;
		_targetHeight = (float)targetTexture->handle->height;
	}
	_driver->setViewport(0, 0, (unsigned)_targetWidth, (unsigned)_targetHeight, 0, 1);
}

void Canvas::endDraw()
{
}

void Canvas::drawImage(
	roRDriverTexture* texture,
	float dstx, float dsty
)
{
	drawImage(
		texture,
		0, 0, (float)texture->width, (float)texture->height,
		dstx, dsty, (float)texture->width, (float)texture->height
	);
}

void Canvas::drawImage(
	roRDriverTexture* texture,
	float dstx, float dsty, float dstw, float dsth
)
{
	drawImage(
		texture,
		0, 0, (float)texture->width, (float)texture->height,
		dstx, dsty, dstw, dsth
	);
}

void Canvas::drawImage(
	roRDriverTexture* texture,
	float srcx, float srcy, float srcw, float srch,
	float dstx, float dsty, float dstw, float dsth
)
{
	if(!texture || !texture->width || !texture->height) return;

	const float z = 0;

	float cw = _targetWidth;
	float ch = _targetHeight;
	float invcw = 2.0f / cw;
	float invch = 2.0f / ch;

	float sx1 = srcx, sx2 = srcx + srcw;
	float sy1 = srcy, sy2 = srcy + srch;
	float dx1 = dstx, dx2 = dstx + dstw;
	float dy1 = dsty, dy2 = dsty + dsth;

	// Calculate the texture UV
	float invw = 1.0f / (float)texture->width;
	float invh = 1.0f / (float)texture->height;
	sx1 *= invw; sx2 *= invw;
	sy1 *= invh; sy2 *= invh;

	// Calculate the destination positions in homogeneous coordinate
	dx1 = invcw * dx1 - 1;
	dx2 = invcw * dx2 - 1;
	dy1 = invch * dy1 - 1;
	dy2 = invch * dy2 - 1;

	float vertex[][6] = {
		{dx1, dy1, z, 1,	sx1,sy1},
		{dx1, dy2, z, 1,	sx1,sy2},
		{dx2, dy2, z, 1,	sx2,sy2},
		{dx2, dy1, z, 1,	sx2,sy1}
	};
	roVerify(_driver->updateBuffer(_vBuffer, 0, vertex, sizeof(vertex)));

	roRDriverShader* shaders[] = { _vShader, _pShader };
	roVerify(_driver->bindShaders(shaders, roCountof(shaders)));
	roVerify(_driver->bindShaderInput(_inputLayout.typedPtr(), _inputLayout.size(), NULL));

	_driver->setTextureState(&_textureState, 1, 0);
	roVerify(_driver->setUniformTexture(stringHash("tex"), texture));

	_driver->drawTriangleIndexed(0, 6, 0);
}

}	// namespace ro
