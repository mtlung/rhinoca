#include "pch.h"
#include "roCanvas.h"
#include "roFont.h"
#include "shivavg/openvg.h"
#include "shivavg/vgu.h"
#include "shivavg/shContext.h"
#include "../base/roCpuProfiler.h"
#include "../base/roLog.h"
#include "../base/roParser.h"
#include "../base/roStringHash.h"
#include "../base/roTypeCast.h"
#include "../roSubSystems.h"

extern void updateBlendingStateGL(VGContext *c, int alphaIsOne);

namespace ro {

static DefaultAllocator _allocator;

Canvas::Canvas()
	: _driver(NULL), _context(NULL)
	, _vBuffer(NULL), _uBuffer(NULL)
	, _vShader(NULL), _pShader(NULL)
	, _targetWidth(0), _targetHeight(0)
	, _isBatchMode(false), _batchedQuadCount(0)
	, _mappedVBuffer(NULL)
	, _openvg(NULL)
{
}

Canvas::~Canvas()
{
	roAssert(!_isBatchMode);
	roAssert(!_mappedVBuffer);
	destroy();
}

struct Canvas::OpenVG
{
	VGPath path;			/// The path for generic use
	bool pathEmpty;
	VGPath pathSimpleShape;	/// The path allocated for simple shape usage
	VGPaint strokePaint;
	VGPaint fillPaint;
};

void Canvas::init()
{
	if(!roSubSystems || !roSubSystems->renderContext) {
		roAssert(false);
		return;
	}

	_context = roSubSystems->renderContext;
	_driver = _context->driver;

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
		"uniform constants { vec4 globalColor; bool isRtTexture; bool isAlphaTexture; bool isGrayScaleTexture; };"
		"in vec4 position;"
		"in vec2 texCoord;"
		"out varying vec2 _texCoord;"
		"void main(void) {"
		"	position.y = -position.y; if(isRtTexture) texCoord.y = 1-texCoord.y;\n"	// Flip y axis
		"	gl_Position = position;  _texCoord = texCoord;"
		"}",

		// HLSL
		"cbuffer constants { float4 globalColor; bool isRtTexture; bool isAlphaTexture; bool isGrayScaleTexture; }"
		"struct VertexInputType { float4 pos : POSITION; float2 texCoord : TEXCOORD0; };"
		"struct PixelInputType { float4 pos : SV_POSITION; float2 texCoord : TEXCOORD0; };"
		"PixelInputType main(VertexInputType input) {"
		"	PixelInputType output; output.pos = input.pos; output.texCoord = input.texCoord;"
		"	output.pos.y = -output.pos.y;"	// Flip y axis
		"	return output;"
		"}"
	};

	static const char* pShaderSrc[] = 
	{
		// GLSL
		"uniform constants { vec4 globalColor; bool isRtTexture; bool isAlphaTexture; bool isGrayScaleTexture; };"
		"uniform sampler2D tex;"
		"in vec2 _texCoord;"
		"void main(void) { "
		"	vec4 ret = texture2D(tex, _texCoord);"
		"	if(isAlphaTexture) ret.rgb = 1;"
		"	gl_FragColor = globalColor * ret;"
		"}",

		// HLSL
		"cbuffer constants { float4 globalColor; bool isRtTexture; bool isAlphaTexture; bool isGrayScaleTexture; }"
		"Texture2D tex;"
		"SamplerState sampleType;"
		"struct PixelInputType { float4 pos : SV_POSITION; float2 texCoord : TEXCOORD0; };"
		"float4 main(PixelInputType input):SV_Target {"
		"	float4 ret = tex.Sample(sampleType, input.texCoord);"
		"	if(isAlphaTexture) ret.rgb = 1;"
		"	if(isGrayScaleTexture) ret.rgb = ret.r;"
		"	return globalColor * ret;"
		"}"
	};

	roVerify(_driver->initShader(_vShader, roRDriverShaderType_Vertex, &vShaderSrc[driverIndex], 1, NULL, NULL));
	roVerify(_driver->initShader(_pShader, roRDriverShaderType_Pixel, &pShaderSrc[driverIndex], 1, NULL, NULL));

	// Create vertex buffer
	_vBuffer = _driver->newBuffer();
	roVerify(_driver->initBuffer(_vBuffer, roRDriverBufferType_Vertex, roRDriverDataUsage_Stream, NULL, 4 * 6 * sizeof(float)));

	_iBuffer = _driver->newBuffer();
	roVerify(_driver->initBuffer(_iBuffer, roRDriverBufferType_Index, roRDriverDataUsage_Stream, NULL, 6 * sizeof(roUint16)));

	// Create uniform buffer
	bool isRtTexture[16*2] = {0};	// In DirectX the size of the const buffer must be multiple of 16
	_uBuffer = _driver->newBuffer();
	roVerify(_driver->initBuffer(_uBuffer, roRDriverBufferType_Uniform, roRDriverDataUsage_Stream, isRtTexture, sizeof(isRtTexture)));

	// Bind shader buffer input
	const roRDriverShaderBufferInput input[] = {
		{ _vBuffer, "position", 0, 0, sizeof(float)*6, 0 },
		{ _vBuffer, "texCoord", 0, sizeof(float)*4, sizeof(float)*6, 0 },
		{ _uBuffer, "constants", 0, 0, 0, 0 },
		{ _iBuffer, "", 0, 0, 0, 0 },
	};
	roAssert(roCountof(input) == _bufferInputs.size());
	for(roSize i=0; i<roCountof(input); ++i)
		_bufferInputs[i] = input[i];

	_textureInput.name = "tex";
	_textureInput.nameHash = 0;
	_textureInput.shader = _pShader;
	_textureInput.texture = NULL;

	// Set the texture state
	roRDriverTextureState textureState =  {
		0,
		roRDriverTextureFilterMode_MinMagLinear,
		roRDriverTextureAddressMode_Repeat, roRDriverTextureAddressMode_Repeat
	};
	_textureState = textureState;

	// Init _openvg
	_openvg = new OpenVG;

	roVerify(vgCreateContextSH(1, 1, _context));

	_openvg->path = vgCreatePath(
		VG_PATH_FORMAT_STANDARD, VG_PATH_DATATYPE_F,
		1, 0, 0, 0, VG_PATH_CAPABILITY_ALL
	);

	_openvg->pathSimpleShape = vgCreatePath(
		VG_PATH_FORMAT_STANDARD, VG_PATH_DATATYPE_F,
		1, 0, 0, 0, VG_PATH_CAPABILITY_ALL
	);

	_openvg->strokePaint = vgCreatePaint();
	_openvg->fillPaint = vgCreatePaint();

	vgSeti(VG_FILL_RULE, VG_NON_ZERO);
	vgSetPaint(_openvg->strokePaint, VG_STROKE_PATH);
	vgSetPaint(_openvg->fillPaint, VG_FILL_PATH);

	// Setup initial drawing states
	static const float black[4] = { 0, 0, 0, 1 };
	setLineCap("butt");
	setLineJoin("round");
	setLineWidth(1);
	setGlobalColor(1, 1, 1, 1);
	setComposition("source-over");
	setStrokeColor(black);
	setFillColor(black);
	setIdentity();
	setFont("10pt \"Arial\"");	// Source: https://developer.mozilla.org/en/Drawing_text_using_a_canvas

	// Setup rasterizer state
	_rasterizerState.hash = 0;
	_rasterizerState.scissorEnable = false;
	_rasterizerState.smoothLineEnable = false;
	_rasterizerState.multisampleEnable = false;
	_rasterizerState.isFrontFaceClockwise = false;
	_rasterizerState.cullMode = roRDriverCullMode_None;

	resetClip();
}

bool Canvas::initTargetTexture(unsigned width, unsigned height)
{
	targetTexture = new Texture("");
	targetTexture->handle = _driver->newTexture();
	if(!_driver->initTexture(targetTexture->handle, width, height, 1, roRDriverTextureFormat_RGBA, roRDriverTextureFlag_RenderTarget))
		return false;
	if(!_driver->updateTexture(targetTexture->handle, 0, 0, NULL, 0, NULL))
		return false;

	depthStencilTexture = new Texture("");
	depthStencilTexture->handle = _driver->newTexture();
	if(!_driver->initTexture(depthStencilTexture->handle, width, height, 1, roRDriverTextureFormat_DepthStencil, roRDriverTextureFlag_None))
		return false;
	if(!_driver->updateTexture(depthStencilTexture->handle, 0, 0, NULL, 0, NULL))
		return false;

	// TODO: It would be nice if we can keep the content during resize
	clearRect(0, 0, (float)width, (float)height);

	return true;
}

void Canvas::destroy()
{
	if(!_context || !_driver) return;
	_driver->deleteBuffer(_vBuffer);
	_driver->deleteBuffer(_iBuffer);
	_driver->deleteBuffer(_uBuffer);
	_driver->deleteShader(_vShader);
	_driver->deleteShader(_pShader);
	_driver = NULL;
	_context = NULL;
	_vBuffer = _uBuffer = NULL;
	_vShader = _pShader = NULL;

	vgDestroyPath(_openvg->path);
	vgDestroyPath(_openvg->pathSimpleShape);
	vgDestroyPaint(_openvg->strokePaint);
	vgDestroyPaint(_openvg->fillPaint);

	delete _openvg;

	vgDestroyContextSH();

	if(roSubSystems)
		roSubSystems->currentCanvas = NULL;
}

void Canvas::makeCurrent()
{
	bool targetSizeChanged = false;
	if(!targetTexture || !targetTexture->handle) {
		roVerify(_driver->setRenderTargets(NULL, 0, false));

		targetSizeChanged = _targetWidth != _context->width || _targetHeight != _context->height;
		_targetWidth = _context->width;
		_targetHeight = _context->height;
	}
	else {
		roRDriverTexture* tex[] = { targetTexture->handle, depthStencilTexture->handle };
		roVerify(_driver->setRenderTargets(tex, roCountof(tex), false));

		targetSizeChanged = _targetWidth != targetTexture->handle->width || _targetHeight != targetTexture->handle->height;
		_targetWidth = targetTexture->handle->width;
		_targetHeight = targetTexture->handle->height;
	}

	_driver->setRasterizerState(&_rasterizerState);

	roAssert(roSubSystems);
	if(roSubSystems->currentCanvas == this && !targetSizeChanged)
		return;

	vgResizeSurfaceSH((VGint)_targetWidth, (VGint)_targetHeight);
	_orthoMat = makeOrthoMat4(0, (float)_targetWidth, 0, (float)_targetHeight, 0, 1);
	_driver->adjustDepthRangeMatrix(_orthoMat.data);

	vgSetPaint(_openvg->strokePaint, VG_STROKE_PATH);
	vgSetPaint(_openvg->fillPaint, VG_FILL_PATH);

	roSubSystems->currentCanvas = this;
}

void Canvas::clearRect(float x, float y, float w, float h)
{
	vgClearPath(_openvg->pathSimpleShape, VG_PATH_CAPABILITY_ALL);
	vguRect(_openvg->pathSimpleShape, x, y, w, h);

	const float black[4] = { 0, 0, 0, 0 };
	vgSetParameterfv(_openvg->fillPaint, VG_PAINT_COLOR, 4, black);

	int orgBlendMode = vgGeti(VG_BLEND_MODE);
	vgSeti(VG_BLEND_MODE, VG_BLEND_SRC);

	makeCurrent();
	vgLoadIdentity();
	vgDrawPath(_openvg->pathSimpleShape, VG_FILL_PATH);

	vgSetParameterfv(_openvg->fillPaint, VG_PAINT_COLOR, 4, _currentState.fillColor);
	vgSeti(VG_BLEND_MODE, orgBlendMode);
}


// ----------------------------------------------------------------------

void Canvas::save()
{
	_stateStack.pushBack(_currentState);
}

void Canvas::restore()
{
	if(!_stateStack.isEmpty()) {	// Be more forgiving
		_currentState = _stateStack.back();
		_stateStack.popBack();
	}

	setGlobalColor(_currentState.globalColor[0], _currentState.globalColor[1], _currentState.globalColor[2], _currentState.globalColor[3]);
	setLineWidth(_currentState.lineWidth);

	vgSeti(VG_STROKE_CAP_STYLE,  _currentState.lineCap);
	vgSeti(VG_STROKE_JOIN_STYLE, _currentState.lineJoin);

	setStrokeColor(_currentState.strokeColor);
	setFillColor(_currentState.fillColor);

	float rect[4];
	roMemcpy(rect, &_currentState.clipRect, sizeof(rect));
	if(_currentState.enableClip)
		clipRect(rect[0], rect[1], rect[2], rect[3]);
	else
		resetClip();

	vgSeti(VG_BLEND_MODE, _currentState.compisitionOperation);
}


// ----------------------------------------------------------------------

void Canvas::drawImage(roRDriverTexture* texture, float dstx, float dsty)
{
	drawImage(
		texture,
		0, 0, (float)texture->width, (float)texture->height,
		dstx, dsty, (float)texture->width, (float)texture->height
	);
}

void Canvas::drawImage(roRDriverTexture* texture, float dstx, float dsty, float dstw, float dsth)
{
	drawImage(
		texture,
		0, 0, (float)texture->width, (float)texture->height,
		dstx, dsty, dstw, dsth
	);
}

void Canvas::_flushDrawImageBatch()
{
	if(_mappedVBuffer)
		_driver->unmapBuffer(_vBuffer);
	_mappedVBuffer = NULL;

	if(_batchedQuadCount == 0)
		return;

	for(roSize i=0; i<_perTextureQuadList.size(); ++i)
	{
		// Resize the index buffer if needed
		roSize indexBufSize = _perTextureQuadList[i].quadCount * 6 * sizeof(roUint16);
		if(_iBuffer->sizeInBytes <= indexBufSize)
			roVerify(_driver->resizeBuffer(_iBuffer, indexBufSize));

		// Build the index buffer
		roUint16 iIdx = 0;
		ro::Array<PerTextureQuadList::Range>& ranges = _perTextureQuadList[i].range;
		_mappedIBuffer = (roUint16*)_driver->mapBuffer(_iBuffer, roRDriverMapUsage_Write, 0, indexBufSize);
		for(roSize j=0; j<ranges.size(); ++j) {
			for(roUint16 k=ranges[j].begin; k<ranges[j].end; ++k) {
				_mappedIBuffer[iIdx++] = k * 4 + 0;
				_mappedIBuffer[iIdx++] = k * 4 + 1;
				_mappedIBuffer[iIdx++] = k * 4 + 2;
				_mappedIBuffer[iIdx++] = k * 4 + 1;
				_mappedIBuffer[iIdx++] = k * 4 + 2;
				_mappedIBuffer[iIdx++] = k * 4 + 3;
			}
		}
		_driver->unmapBuffer(_iBuffer);

		_drawImageDrawcall(_perTextureQuadList[i].tex, _perTextureQuadList[i].quadCount);
	}

	_batchedQuadCount = 0;
	_perTextureQuadList.clear();
}

void Canvas::_drawImageDrawcall(roRDriverTexture* texture, roSize quadCount)
{
	makeCurrent();

	// Shader constants
	struct Constants {
		Vec4 color;
		roInt32 isRtTexture;
		roInt32 isAlphaTexture;
		roInt32 isGrayScaleTexture;
	};
	Constants constants = {
		Vec4(_currentState.globalColor),
		texture->flags & roRDriverTextureFlag_RenderTarget,
		texture->format == roRDriverTextureFormat_A,
		texture->format == roRDriverTextureFormat_L,
	};
	roVerify(_driver->updateBuffer(_uBuffer, 0, &constants, sizeof(constants)));

	// Shaders
	roRDriverShader* shaders[] = { _vShader, _pShader };
	roVerify(_driver->bindShaders(shaders, roCountof(shaders)));
	roVerify(_driver->bindShaderBuffers(_bufferInputs.typedPtr(), _bufferInputs.size(), NULL));

	// Texture
	_driver->setTextureState(&_textureState, 1, 0);
	_textureInput.texture = texture;
	roVerify(_driver->bindShaderTextures(&_textureInput, 1));

	// Blend state
	updateBlendingStateGL(shGetContext(), 0);	// TODO: It would be an optimization to know the texture has transparent or not

	_driver->drawPrimitiveIndexed(roRDriverPrimitiveType_TriangleList, 0, quadCount * 6, 0);
}

// TODO: There are still rooms for optimization
void Canvas::drawImage(roRDriverTexture* texture, float srcx, float srcy, float srcw, float srch, float dstx, float dsty, float dstw, float dsth)
{
	CpuProfilerScope cpuProfilerScope(__FUNCTION__);

	if(!texture || !texture->width || !texture->height || globalAlpha() <= 0) return;
	if(srcw <= 0 || srch <= 0 || dstw <= 0 || dsth <= 0) return;

	const float z = 0;

	float sx1 = srcx, sx2 = srcx + srcw;
	float sy1 = srcy, sy2 = srcy + srch;
	float dx1 = dstx, dx2 = dstx + dstw;
	float dy1 = dsty, dy2 = dsty + dsth;

	// Calculate the texture UV
	float invw = 1.0f / (float)texture->width;
	float invh = 1.0f / (float)texture->height;
	sx1 *= invw; sx2 *= invw;
	sy1 *= invh; sy2 *= invh;

	// Adjust UV so that the sampling position after interpolation is at the center,
	// making a perfect pixel transfer even src size and dst size are not the same.
	float centriodOffsetx = 0.5f * invw * (1 - srcw / dstw);
	float centriodOffsety = 0.5f * invh * (1 - srch / dsth);
	sx1 += centriodOffsetx;
	sx2 -= centriodOffsetx;
	sy1 += centriodOffsety;
	sy2 -= centriodOffsety;

	float vertex[4][6] = {
		{dx1, dy1, z, 1,	sx1,sy1},
		{dx2, dy1, z, 1,	sx2,sy1},
		{dx1, dy2, z, 1,	sx1,sy2},
		{dx2, dy2, z, 1,	sx2,sy2},
	};

	// Transform the vertex using the current transform
	Mat4 mat44 = _orthoMat * _currentState.transform;
	for(roSize i=0; i<4; ++i)
		mat4MulVec3(mat44.mat, vertex[i], vertex[i]);

	if(_isBatchMode) {
		// Search for the cache entry
		PerTextureQuadList* listEntry = NULL;
		for(roSize i=0; i<_perTextureQuadList.size(); ++i) {
			if(_perTextureQuadList[i].tex == texture) {
				listEntry = &_perTextureQuadList[i];
				break;
			}
		}
		if(!listEntry) {
			listEntry = &_perTextureQuadList.pushBack();
			listEntry->tex = texture;
			listEntry->quadCount = 0;
		}
		listEntry->quadCount++;

		// Insert into the vertex range
		if(!listEntry->range.isEmpty() && listEntry->range.back().end == _batchedQuadCount)
			listEntry->range.back().end++;
		else {
			PerTextureQuadList::Range range = { _batchedQuadCount, _batchedQuadCount + 1 };
			listEntry->range.pushBack(range);
		}

		// Check if larger vertex buffer is needed
		if(_vBuffer->sizeInBytes <= (_batchedQuadCount + 1) * sizeof(vertex)) {
			_driver->unmapBuffer(_vBuffer);
			roSize larger = roMaxOf2(_vBuffer->sizeInBytes * 2, (_batchedQuadCount + 1) * roSize(sizeof(vertex)));
			roVerify(_driver->resizeBuffer(_vBuffer, larger));
			_mappedVBuffer = (char*)_driver->mapBuffer(_vBuffer, roRDriverMapUsage_ReadWrite, 0, _vBuffer->sizeInBytes);
		}

		roMemcpy(_mappedVBuffer + _batchedQuadCount * sizeof(vertex), vertex, sizeof(vertex));
		++_batchedQuadCount;
	}
	else {
		roAssert(_batchedQuadCount == 0);

		roVerify(_driver->updateBuffer(_vBuffer, 0, vertex, sizeof(vertex)));

		roUint16 index[] = { 0, 1, 2, 1, 2, 3 };
		roVerify(_driver->updateBuffer(_iBuffer, 0, index, sizeof(index)));

		_drawImageDrawcall(texture, 1);
	}
}


// ----------------------------------------------------------------------

void Canvas::beginDrawImageBatch()
{
	_isBatchMode = true;
	_batchedQuadCount = 0;

	roAssert(!_mappedVBuffer);

	_mappedVBuffer = (char*)_driver->mapBuffer(_vBuffer, roRDriverMapUsage_Write, 0, _vBuffer->sizeInBytes);
}

void Canvas::endDrawImageBatch()
{
	_flushDrawImageBatch();
	_isBatchMode = false;
}


// ----------------------------------------------------------------------

const roUint8* Canvas::lockPixelRead(roSize& rowBytes)
{
	if(!targetTexture)
		return NULL;

	return (roUint8*)_driver->mapTexture(targetTexture->handle, roRDriverMapUsage_Read, 0, 0, rowBytes);
}

roUint8* Canvas::lockPixelWrite(roSize& rowBytes)
{
	if(!targetTexture)
		return NULL;

	return (roUint8*)_driver->mapTexture(targetTexture->handle, roRDriverMapUsage_Write, 0, 0, rowBytes);
}

void Canvas::unlockPixelData()
{
	if(!targetTexture)
		return;

	makeCurrent();

	_driver->unmapTexture(targetTexture->handle, 0, 0);
}


// ----------------------------------------------------------------------

void Canvas::scale(float x, float y)
{
	float v[3] = { x, y, 1 };
	Mat4 m = makeScaleMat4(v);
	_currentState.transform *= m;
}

void Canvas::rotate(float clockwiseRadian)
{
	float axis[3] = { 0, 0, 1 };
	Mat4 m = makeAxisRotationMat4(axis, clockwiseRadian);
	_currentState.transform *= m;
}

void Canvas::translate(float x, float y)
{
	float v[3] = { x, y, 0 };
	Mat4 m = makeTranslationMat4(v);
	_currentState.transform *= m;
}

void Canvas::transform(float m11, float m12, float m21, float m22, float dx, float dy)
{
	Mat4 m(mat4Identity);
	m.m11 = m11;	m.m12 = m12;
	m.m21 = m21;	m.m22 = m22;
	m.m03 = dx;		m.m13 = dy;
	_currentState.transform *= m;
}

void Canvas::transform(float mat44[16])
{
	_currentState.transform *= *reinterpret_cast<Mat4*>(mat44);
}

void Canvas::setTransform(float m11, float m12, float m21, float m22, float dx, float dy)
{
	Mat4 m(mat4Identity);
	m.m11 = m11;	m.m12 = m12;
	m.m21 = m21;	m.m22 = m22;
	m.m03 = dx;		m.m13 = dy;
	_currentState.transform = m;
}

void Canvas::setIdentity()
{
	_currentState.transform.identity();
}

void Canvas::setTransform(float mat44[16])
{
	_currentState.transform.copyFrom(mat44);
}


// ----------------------------------------------------------------------

void Canvas::beginPath()
{
	vgClearPath(_openvg->path, VG_PATH_CAPABILITY_ALL);
	_openvg->pathEmpty = true;
}

void Canvas::closePath()
{
	VGubyte seg = VG_CLOSE_PATH;
	VGfloat data[] = { 0, 0 };
	vgAppendPathData(_openvg->path, 1, &seg, data);
}

void Canvas::moveTo(float x, float y)
{
	VGubyte seg = VG_MOVE_TO;
	VGfloat data[] = { x, y };
	vgAppendPathData(_openvg->path, 1, &seg, data);

	_openvg->pathEmpty = false;
}

void Canvas::lineTo(float x, float y)
{
	VGubyte seg = _openvg->pathEmpty ? VG_MOVE_TO : VG_LINE_TO_ABS;
	VGfloat data[] = { x, y };
	vgAppendPathData(_openvg->path, 1, &seg, data);

	_openvg->pathEmpty = false;
}

void Canvas::quadraticCurveTo(float cpx, float cpy, float x, float y)
{
	VGubyte seg = VG_QUAD_TO;
	VGfloat data[] = { cpx, cpy, x, y };
	vgAppendPathData(_openvg->path, 1, &seg, data);

	_openvg->pathEmpty = false;
}

void Canvas::bezierCurveTo(float cp1x, float cp1y, float cp2x, float cp2y, float x, float y)
{
	VGubyte seg = VG_CUBIC_TO;
	VGfloat data[] = { cp1x, cp1y, cp2x, cp2y, x, y };
	vgAppendPathData(_openvg->path, 1, &seg, data);

	_openvg->pathEmpty = false;
}

void Canvas::arcTo(float x1, float y1, float x2, float y2, float radius)
{
	// TODO: To be implement
}

void Canvas::arc(float x, float y, float radius, float startAngle, float endAngle, bool anticlockwise)
{
	startAngle = startAngle * 360 / (2 * 3.14159f);
	endAngle = endAngle * 360 / (2 * 3.14159f);
	radius *= 2;
	vguArc(_openvg->path, x,y, radius,radius, startAngle, (anticlockwise ? -1 : 1) * (endAngle-startAngle), VGU_ARC_OPEN);
}

void Canvas::rect(float x, float y, float w, float h)
{
	vguRect(_openvg->path, x, y, w, h);
	moveTo(x, y);
}


// ----------------------------------------------------------------------

struct Gradient
{
	struct ColorStop {
		float t, r, g, b, a;
	};

	Gradient() {}
	void* handle;
	typedef TinyArray<ColorStop, 4> ColorStops;
	ColorStops stops;
	float radiusStart, radiusEnd;	// For radial fill
};

void* Canvas::createLinearGradient(float xStart, float yStart, float xEnd, float yEnd)
{
	Gradient* ret = _allocator.newObj<Gradient>().unref();
	roZeroMemory(ret, sizeof(*ret));

	ret->handle = vgCreatePaint();
	vgSetParameteri(ret->handle, VG_PAINT_COLOR_RAMP_SPREAD_MODE, VG_COLOR_RAMP_SPREAD_PAD);
	vgSetParameteri(ret->handle, VG_PAINT_TYPE, VG_PAINT_TYPE_LINEAR_GRADIENT);
	const float linear[] = { xStart, yStart ,xEnd, yEnd };
	vgSetParameterfv(ret->handle, VG_PAINT_LINEAR_GRADIENT, 4, linear);

	return ret;
}

void* Canvas::createRadialGradient(float xStart, float yStart, float radiusStart, float xEnd, float yEnd, float radiusEnd)
{
	Gradient* ret = _allocator.newObj<Gradient>().unref();
	roZeroMemory(ret, sizeof(*ret));

	ret->handle = vgCreatePaint();
	vgSetParameteri(ret->handle, VG_PAINT_COLOR_RAMP_SPREAD_MODE, VG_COLOR_RAMP_SPREAD_PAD);
	vgSetParameteri(ret->handle, VG_PAINT_TYPE, VG_PAINT_TYPE_RADIAL_GRADIENT);
	ret->radiusStart = radiusStart;
	ret->radiusEnd = radiusEnd;
	const float radial[] = { xEnd, yEnd, xStart, yStart ,radiusEnd };
	vgSetParameterfv(ret->handle, VG_PAINT_RADIAL_GRADIENT, 5, radial);

	return ret;
}

void Canvas::addGradientColorStop(void* gradient, float offset, float r, float g, float b, float a)
{
	Gradient* grad = reinterpret_cast<Gradient*>(gradient);
	Gradient::ColorStops& stops = grad->stops;

	if(!gradient) return;

	// We need to adjust the offset for radial fill, to deal with starting radius
	if(vgGetParameteri(grad->handle, VG_PAINT_TYPE) == VG_PAINT_TYPE_RADIAL_GRADIENT)
	{
		// Add an extra stop as the starting radius
		if(stops.isEmpty()) {
			stops.incSize(1);
			stops.back().t = grad->radiusStart / grad->radiusEnd;
			stops.back().r = r;
			stops.back().g = g;
			stops.back().b = b;
			stops.back().a = a;
		}

		offset *= 1.0f - grad->radiusStart / grad->radiusEnd;
		offset += grad->radiusStart / grad->radiusEnd;
	}

	stops.incSize(1);
	stops.back().t = offset;
	stops.back().r = r;
	stops.back().g = g;
	stops.back().b = b;
	stops.back().a = a;
}

void Canvas::destroyGradient(void* gradient)
{
	Gradient* g = reinterpret_cast<Gradient*>(gradient);
	if(!g) return;
	vgDestroyPaint(g->handle);
	_allocator.deleteObj(g);
}


// ----------------------------------------------------------------------

void Canvas::stroke()
{
	CpuProfilerScope cpuProfilerScope(__FUNCTION__);

	const Mat4& m = _currentState.transform;
	float mat33[] = {
		m.m00, m.m10, m.m20,
		m.m01, m.m11, m.m21,
		m.m03, m.m13, m.m33,
	};

	makeCurrent();
	vgLoadMatrix(mat33);
	vgDrawPath(_openvg->path, VG_STROKE_PATH);
}

void Canvas::strokeRect(float x, float y, float w, float h)
{
	vgClearPath(_openvg->pathSimpleShape, VG_PATH_CAPABILITY_ALL);
	vguRect(_openvg->pathSimpleShape, x, y, w, h);

	const Mat4& m = _currentState.transform;
	float mat33[] = {
		m.m00, m.m10, m.m20,
		m.m01, m.m11, m.m21,
		m.m03, m.m13, m.m33,
	};

	makeCurrent();
	vgLoadMatrix(mat33);
	vgDrawPath(_openvg->pathSimpleShape, VG_STROKE_PATH);
}

void Canvas::getStrokeColor(float* rgba)
{
	vgGetParameterfv(_openvg->strokePaint, VG_PAINT_COLOR, 4, rgba);
}

void Canvas::setStrokeColor(const float* rgba)
{
	float color[4];
	roMemcpy(color, rgba, sizeof(color));

	color[0] *= _currentState.globalColor[0];
	color[1] *= _currentState.globalColor[1];
	color[2] *= _currentState.globalColor[2];
	color[3] *= _currentState.globalColor[3];

	vgSetParameterfv(_openvg->strokePaint, VG_PAINT_COLOR, 4, color);
	roMemcpy(_currentState.strokeColor, rgba, sizeof(_currentState.strokeColor));
}

void Canvas::setStrokeColor(float r, float g, float b, float a)
{
	float color[4] = { r, g, b, a };
	setStrokeColor(color);
}

void Canvas::setLineCap(const char* cap)
{
	struct Cap {
		const char* name;
		VGCapStyle style;
	};

	static const Cap caps[] = {
		{ "butt",	VG_CAP_BUTT },
		{ "round",	VG_CAP_ROUND },
		{ "square",	VG_CAP_SQUARE },
	};

	for(roSize i=0; i<roCountof(caps); ++i)
		if(roStrCaseCmp(cap, caps[i].name) == 0) {
			_currentState.lineCap = caps[i].style;
			vgSeti(VG_STROKE_CAP_STYLE,  caps[i].style);
		}
}

void Canvas::setLineJoin(const char* join)
{
	struct Join {
		const char* name;
		VGJoinStyle style;
	};

	static const Join joins[] = {
		{ "miter",	VG_JOIN_MITER },
		{ "round",	VG_JOIN_ROUND },
		{ "bevel",	VG_JOIN_BEVEL },
	};

	for(roSize i=0; i<roCountof(joins); ++i)
		if(roStrCaseCmp(join, joins[i].name) == 0) {
			_currentState.lineJoin = joins[i].style;
			vgSeti(VG_STROKE_JOIN_STYLE, joins[i].style);
		}
}

void Canvas::setLineWidth(float width)
{
	vgSetf(VG_STROKE_LINE_WIDTH, width);
	_currentState.lineWidth = width;
}

void Canvas::setStrokeGradient(void* gradient)
{
	Gradient* grad = reinterpret_cast<Gradient*>(gradient);
	vgSetParameterfv(
		grad->handle, VG_PAINT_COLOR_RAMP_STOPS,
		num_cast<VGint>(grad->stops.size() * 5), (float*)grad->stops.typedPtr()
	);
	vgSetPaint(grad->handle, VG_STROKE_PATH);
}


// ----------------------------------------------------------------------

void Canvas::fill()
{
	CpuProfilerScope cpuProfilerScope(__FUNCTION__);

	const Mat4& m = _currentState.transform;
	float mat33[] = {
		m.m00, m.m10, m.m20,
		m.m01, m.m11, m.m21,
		m.m03, m.m13, m.m33,
	};

	makeCurrent();
	vgLoadMatrix(mat33);
	vgDrawPath(_openvg->path, VG_FILL_PATH);
}

void Canvas::fillRect(float x, float y, float w, float h)
{
	CpuProfilerScope cpuProfilerScope(__FUNCTION__);

	vgClearPath(_openvg->pathSimpleShape, VG_PATH_CAPABILITY_ALL);
	vguRect(_openvg->pathSimpleShape, x, y, w, h);

	const Mat4& m = _currentState.transform;
	float mat33[] = {
		m.m00, m.m10, m.m20,
		m.m01, m.m11, m.m21,
		m.m03, m.m13, m.m33,
	};

	makeCurrent();
	vgLoadMatrix(mat33);
	vgDrawPath(_openvg->pathSimpleShape, VG_FILL_PATH);
}

void Canvas::fillText(const char* utf8Str, float x, float y, float maxWidth)
{
	CpuProfilerScope cpuProfilerScope(__FUNCTION__);

	if(!roSubSystems || !roSubSystems->fontMgr) return;

	if(FontPtr font = roSubSystems->fontMgr->getFont(_currentState.fontName.c_str())) {
		makeCurrent();
		font->setStyle(_currentState.fontStyle.c_str());
		font->draw(utf8Str, roSize(-1), x, y, maxWidth, *this);
	}
	else
		roLog("warn", "Fail to find font resource for typeface:%s\n", _currentState.fontName.c_str());
}

void Canvas::measureText(const roUtf8* str, roSize maxStrLen, float maxWidth, TextMetrics& metrics)
{
	if(FontPtr font = roSubSystems->fontMgr->getFont(_currentState.fontName.c_str())) {
		font->setStyle(_currentState.fontStyle.c_str());
		font->measure(str, maxStrLen, maxWidth, metrics);
	}
	else
		metrics = TextMetrics();
}

float Canvas::lineSpacing()
{
	if(FontPtr font = roSubSystems->fontMgr->getFont(_currentState.fontName.c_str())) {
		font->setStyle(_currentState.fontStyle.c_str());
		return font->getLineSpacing();
	}
	else
		return 0;
}

void Canvas::getFillColor(float* rgba)
{
	vgGetParameterfv(_openvg->fillPaint, VG_PAINT_COLOR, 4, rgba);
}

void Canvas::setFillColor(const float* rgba)
{
	float color[4];
	roMemcpy(color, rgba, sizeof(color));

	color[0] *= _currentState.globalColor[0];
	color[1] *= _currentState.globalColor[1];
	color[2] *= _currentState.globalColor[2];
	color[3] *= _currentState.globalColor[3];

	vgSetParameterfv(_openvg->fillPaint, VG_PAINT_COLOR, 4, color);
	roMemcpy(_currentState.fillColor, rgba, sizeof(_currentState.fillColor));
}

void Canvas::setFillColor(float r, float g, float b, float a)
{
	float color[4] = { r, g, b, a };
	setFillColor(color);
}

void Canvas::setFillGradient(void* gradient)
{
	Gradient* grad = reinterpret_cast<Gradient*>(gradient);
	vgSetParameterfv(
		grad->handle, VG_PAINT_COLOR_RAMP_STOPS,
		num_cast<VGint>(grad->stops.size() * 5), (float*)grad->stops.typedPtr()
	);
	vgSetPaint(grad->handle, VG_FILL_PATH);
}

void Canvas::clipRect(float x, float y, float w, float h)
{
	_rasterizerState.hash = 0;
	_rasterizerState.scissorEnable = true;

	// Take intersection with the current clip rect
	float right1 = x + w;
	float bottom1 = y + h;
	float right2 = _currentState.clipRect[0] + _currentState.clipRect[2];
	float bottom2 = _currentState.clipRect[1] + _currentState.clipRect[3];
	float right = roMinOf2(right1, right2);
	float bottom = roMinOf2(bottom1, bottom2);
	x = roMaxOf2(x, _currentState.clipRect[0]);
	y = roMaxOf2(y, _currentState.clipRect[1]);
	w = roMaxOf2(right - x, 0.f);
	h = roMaxOf2(bottom - y, 0.f);

	float rect[4] = { x, y, w, h };
	roMemcpy(_currentState.clipRect, rect, sizeof(rect));
	_currentState.enableClip = true;

	_driver->setScissorRect(int(x), int(y), int(w), int(h));
}

void Canvas::getClipRect(float* rect_xywh)
{
	if(rect_xywh)
		roMemcpy(rect_xywh, _currentState.clipRect, sizeof(_currentState.clipRect));
}

void Canvas::clip()
{
	// TODO: Implement
	roAssert(false);

	_currentState.enableClip = true;
}

void Canvas::resetClip()
{
	_rasterizerState.hash = 0;
	_rasterizerState.scissorEnable = false;

	_currentState.enableClip = false;
	_currentState.clipRect[0] = 0;
	_currentState.clipRect[1] = 0;
	_currentState.clipRect[2] = (float)width();
	_currentState.clipRect[3] = (float)height();
}

// ----------------------------------------------------------------------

unsigned Canvas::width() const {
	return _targetWidth;
}

unsigned Canvas::height() const {
	return _targetHeight;
}

float Canvas::globalAlpha() const {
	return _currentState.globalColor[3];
}

void Canvas::setGlobalAlpha(float alpha)
{
	setGlobalColor(1, 1, 1, alpha);
}

void Canvas::getglobalColor(float* rgba)
{
	rgba[0] = _currentState.globalColor[0];
	rgba[1] = _currentState.globalColor[1];
	rgba[2] = _currentState.globalColor[2];
	rgba[3] = _currentState.globalColor[3];
}

void Canvas::setGlobalColor(const float* rgba)
{
	setGlobalColor(rgba[0], rgba[1], rgba[2], rgba[3]);
}

void Canvas::setGlobalColor(float r, float g, float b, float a)
{
	float sc[4];
	roMemcpy(sc, _currentState.strokeColor, sizeof(sc));

	float fc[4];
	roMemcpy(fc, _currentState.fillColor, sizeof(fc));

	sc[0] *= r; sc[1] *= g; sc[2] *= b; sc[3] *= a;
	fc[0] *= r; fc[1] *= g; fc[2] *= b; fc[3] *= a;

	vgSetParameterfv(_openvg->strokePaint, VG_PAINT_COLOR, 4, sc);
	vgSetParameterfv(_openvg->fillPaint, VG_PAINT_COLOR, 4, fc);

	_currentState.globalColor[0] = r;
	_currentState.globalColor[1] = g;
	_currentState.globalColor[2] = b;
	_currentState.globalColor[3] = a;
}

static void fontParserCallback(Parsing::ParserResult* result, Parsing::Parser* parser)
{
	Canvas* canvas = reinterpret_cast<Canvas*>(parser->userdata);

	// Extract the font family, which is important for knowing which font resource to use
	if(roStrCmp(result->type, "fontFamily") == 0) {
		if(*result->begin == '"')
			result->begin++;
		if(*(result->end - 1) == '"')
			result->end--;
		canvas->_currentState.fontName = ConstString(result->begin, result->end - result->begin);
	}
}

void Canvas::setFont(const char* style)
{
	using namespace Parsing;
	Parser parser(style, style + roStrLen(style), fontParserCallback, this);
	if(!ro::Parsing::font(&parser).once())
		roLog("warn", "Fail to parse font style:%s\n", style);
	_currentState.fontStyle = style;
}

const char*	Canvas::font() const
{
	return _currentState.fontStyle.c_str();
}

void Canvas::setTextAlign(const char* align)
{
	_currentState.textAlignment = align;
}

const char*	Canvas::textAlign() const
{
	return _currentState.textAlignment.c_str();
}

void Canvas::setTextBaseline(const char* baseLine)
{
	_currentState.textBaseline = baseLine;
}

const char*	Canvas::textBaseline() const
{
	return _currentState.textBaseline.c_str();
}

struct CompositionMapping { StringHash h; VGBlendMode mode; };
static const CompositionMapping _compositionMapping[] = {
	{ stringHash("source-atop"),		VG_BLEND_SRC_ATOP_SH	},
	{ stringHash("source-in"),			VG_BLEND_SRC_IN			},
	{ stringHash("source-out"),			VG_BLEND_SRC_OUT_SH		},
	{ stringHash("source-over"),		VG_BLEND_SRC_OVER		},
	{ stringHash("destination-atop"),	VG_BLEND_DST_ATOP_SH	},
	{ stringHash("destination-in"),		VG_BLEND_DST_IN			},
	{ stringHash("destination-out"),	VG_BLEND_DST_OUT_SH		},
	{ stringHash("destination-over"),	VG_BLEND_DST_OVER		},
	{ stringHash("lighter"),			VG_BLEND_ADDITIVE		},
	{ stringHash("copy"),				VG_BLEND_SRC			},
	{ stringHash("xor"),				VGBlendMode(-1)			},	// FIXME
};

void Canvas::setComposition(const char* operation)
{
	StringHash h = stringLowerCaseHash(operation);
	for(roSize i=0; i<roCountof(_compositionMapping); ++i) {
		if(_compositionMapping[i].h != h) continue;
		_currentState.compisitionOperation = _compositionMapping[i].mode;
		vgSeti(VG_BLEND_MODE, _compositionMapping[i].mode);
		return;
	}

	roLog("warn", "Wrong parameter given to Canvas::setComposition\n");
}

}	// namespace ro
