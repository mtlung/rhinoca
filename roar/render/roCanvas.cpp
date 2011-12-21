#include "pch.h"
#include "roCanvas.h"

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
	roRDriver* driver = context->driver;

	_vShader = driver->newShader();
	_pShader = driver->newShader();

	static const char* vShaderSrc = 
	{
		// GLSL
		"in vec4 position;"
		"in vec2 texCoord;"
		"out varying vec2 _texCoord;"
		"void main(void) { gl_Position=position; _texCoord = texCoord; }",
	};

	static const char* pShaderSrc = 
	{
		// GLSL
		"uniform sampler2D tex;"
		"in vec2 _texCoord;"
		"void main(void) { gl_FragColor = texture2D(tex, _texCoord); }",
	};

	roVerify(driver->initShader(_vShader, roRDriverShaderType_Vertex, &vShaderSrc, 1));
	roVerify(driver->initShader(_pShader, roRDriverShaderType_Pixel, &pShaderSrc, 1));
}

void Canvas::destroy()
{
	if(!_context) return;
	_context->driver->deleteShader(_vShader);
	_context->driver->deleteShader(_pShader);
	_context = NULL;
	_vShader = NULL;
	_pShader = NULL;
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
}

}	// namespace ro
