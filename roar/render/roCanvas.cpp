#include "pch.h"
#include "roCanvas.h"
#include "shivavg/openvg.h"
#include "shivavg/vgu.h"
#include "shivavg/shContext.h"
#include "../base/roLog.h"
#include "../base/roStringHash.h"
#include "../base/roTypeCast.h"

extern void updateBlendingStateGL(VGContext *c, int alphaIsOne);

namespace ro {

static DefaultAllocator _allocator;

Canvas::Canvas()
	: _driver(NULL), _context(NULL)
	, _vBuffer(NULL), _uBuffer(NULL)
	, _vShader(NULL), _pShader(NULL)
	, _targetWidth(0), _targetHeight(0)
{
}

Canvas::~Canvas()
{
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
		"uniform constants { float alpha; bool isRtTexture; };"
		"in vec4 position;"
		"in vec2 texCoord;"
		"out varying vec2 _texCoord;"
		"void main(void) {"
		"	position.y = -position.y; if(isRtTexture) texCoord.y = 1-texCoord.y;\n"	// Flip y axis
		"	gl_Position = position;  _texCoord = texCoord;"
		"}",

		// HLSL
		"cbuffer constants { float alpha; bool isRtTexture; }"
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
		"uniform constants { float alpha; bool isRtTexture; };"
		"uniform sampler2D tex;"
		"in vec2 _texCoord;"
		"void main(void) { gl_FragColor = texture2D(tex, _texCoord); gl_FragColor.a *= alpha; }",

		// HLSL
		"cbuffer constants { float alpha; bool isRtTexture; }"
		"Texture2D tex;"
		"SamplerState sampleType;"
		"struct PixelInputType { float4 pos : SV_POSITION; float2 texCoord : TEXCOORD0; };"
		"float4 main(PixelInputType input):SV_Target {"
		"	float4 ret = tex.Sample(sampleType, input.texCoord);"
		"	ret.a *= alpha;"
		"	return ret;"
		"}"
	};

	roVerify(_driver->initShader(_vShader, roRDriverShaderType_Vertex, &vShaderSrc[driverIndex], 1));
	roVerify(_driver->initShader(_pShader, roRDriverShaderType_Pixel, &pShaderSrc[driverIndex], 1));

	// Create vertex buffer
	float vertex[][6] = { {-1,1,0,1, 0,1}, {-1,-1,0,1, 0,0}, {1,-1,0,1, 1,0}, {1,1,0,1, 1,1} };
	_vBuffer = _driver->newBuffer();
	roVerify(_driver->initBuffer(_vBuffer, roRDriverBufferType_Vertex, roRDriverDataUsage_Stream, vertex, sizeof(vertex)));

	// Create uniform buffer
	bool isRtTexture[16] = {0};	// Most of the time we use a block of 16 bytes
	_uBuffer = _driver->newBuffer();
	roVerify(_driver->initBuffer(_uBuffer, roRDriverBufferType_Uniform, roRDriverDataUsage_Stream, isRtTexture, sizeof(isRtTexture)));

	// Bind shader buffer input
	const roRDriverShaderBufferInput input[] = {
		{ _vShader, _vBuffer, "position", 0, 0, sizeof(float)*6, 0 },
		{ _vShader, _vBuffer, "texCoord", 0, sizeof(float)*4, sizeof(float)*6, 0 },
		{ _vShader, _uBuffer, "constants", 0, 0, 0, 0 },
		{ _pShader, _uBuffer, "constants", 0, 0, 0, 0 },
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

	roVerify(vgCreateContextSH(1, 1, context));

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

	// Setup initial drawing states
	static const float black[4] = { 0, 0, 0, 1 };
	setLineCap("butt");
	setLineJoin("round");
	setLineWidth(1);
	setGlobalAlpha(1);
	setComposition("source-over");
	setStrokeColor(black);
	setFillColor(black);
	setIdentity();
}

bool Canvas::initTargetTexture(unsigned width, unsigned height)
{
	targetTexture = new Texture("");
	targetTexture->handle = _driver->newTexture();
	if(!_driver->initTexture(targetTexture->handle, width, height, roRDriverTextureFormat_RGBA, roRDriverTextureFlag_RenderTarget))
		return false;
	if(!_driver->updateTexture(targetTexture->handle, 0, NULL, 0, NULL))
		return false;

	depthStencilTexture = new Texture("");
	depthStencilTexture->handle = _driver->newTexture();
	if(!_driver->initTexture(depthStencilTexture->handle, width, height, roRDriverTextureFormat_DepthStencil, roRDriverTextureFlag_None))
		return false;
	if(!_driver->updateTexture(depthStencilTexture->handle, 0, NULL, 0, NULL))
		return false;

	return true;
}

void Canvas::destroy()
{
	if(!_context || !_driver) return;
	_driver->deleteBuffer(_vBuffer);
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
}

// Setup rasterizer state
static roRDriverRasterizerState _rasterizerState = {
	0,
	false,		// scissorEnable
	false,		// smoothLineEnable
	false,		// multisampleEnable
	false,		// isFrontFaceClockwise
	roRDriverCullMode_None	// We don't cull any polygon for Canvas
};

void Canvas::beginDraw()
{
	if(!targetTexture || !targetTexture->handle) {
		roVerify(_driver->setRenderTargets(NULL, 0, false));
		_targetWidth = (float)_context->width;
		_targetHeight = (float)_context->height;
	}
	else {
		roRDriverTexture* tex[] = { targetTexture->handle, depthStencilTexture->handle };
		roVerify(_driver->setRenderTargets(tex, roCountof(tex), false));
		_targetWidth = (float)targetTexture->handle->width;
		_targetHeight = (float)targetTexture->handle->height;
	}

	vgResizeSurfaceSH((VGint)_targetWidth, (VGint)_targetHeight);
	_orthoMat = makeOrthoMat4(0, _targetWidth, 0, _targetHeight, 0, 1);
	_driver->adjustDepthRangeMatrix(_orthoMat.data);

	vgSeti(VG_FILL_RULE, VG_NON_ZERO);
	vgSetPaint(_openvg->strokePaint, VG_STROKE_PATH);
	vgSetPaint(_openvg->fillPaint, VG_FILL_PATH);

	restore();

	_driver->setRasterizerState(&_rasterizerState);
}

void Canvas::endDraw()
{
}

void Canvas::clearRect(float x, float y, float w, float h)
{
	const float black[4] = { 0, 0, 0, 0 };
	vgSetParameterfv(_openvg->fillPaint, VG_PAINT_COLOR, 4, black);

	int orgBlendMode = vgGeti(VG_BLEND_MODE);
	vgSeti(VG_BLEND_MODE, VG_BLEND_SRC);

	fillRect(x, y, w, h);

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

	setGlobalAlpha(_currentState.globalAlpha);
	setLineWidth(_currentState.lineWidth);

	vgSeti(VG_STROKE_CAP_STYLE,  _currentState.lineCap);
	vgSeti(VG_STROKE_JOIN_STYLE, _currentState.lineJoin);

	setStrokeColor(_currentState.strokeColor);
	setFillColor(_currentState.fillColor);

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

void Canvas::drawImage(roRDriverTexture* texture, float srcx, float srcy, float srcw, float srch, float dstx, float dsty, float dstw, float dsth)
{
	if(!texture || !texture->width || !texture->height || _currentState.globalAlpha <= 0) return;

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

	float vertex[][6] = {	// Vertex are arranged in a 'z' order, in order to use TriangleStrip as primitive
		{dx1, dy1, z, 1,	sx1,sy1},
		{dx2, dy1, z, 1,	sx2,sy1},
		{dx1, dy2, z, 1,	sx1,sy2},
		{dx2, dy2, z, 1,	sx2,sy2},
	};

	// Transform the vertex using the current transform
	for(roSize i=0; i<4; ++i) {
		mat4MulVec3(_currentState.transform.mat, vertex[i], vertex[i]);
		mat4MulVec3(_orthoMat.mat, vertex[i], vertex[i]);
	}

	roVerify(_driver->updateBuffer(_vBuffer, 0, vertex, sizeof(vertex)));

	struct Constants { float alpha; bool isRtTexture[4]; };
	Constants constants = { _currentState.globalAlpha, { texture->flags & roRDriverTextureFlag_RenderTarget } };
	roVerify(_driver->updateBuffer(_uBuffer, 0, &constants, sizeof(constants)));

	roRDriverShader* shaders[] = { _vShader, _pShader };
	roVerify(_driver->bindShaders(shaders, roCountof(shaders)));
	roVerify(_driver->bindShaderBuffers(_bufferInputs.typedPtr(), _bufferInputs.size(), NULL));

	_driver->setTextureState(&_textureState, 1, 0);
	_textureInput.texture = texture;
	roVerify(_driver->bindShaderTextures(&_textureInput, 1));

	// Blend state
	updateBlendingStateGL(shGetContext(), 0);	// TODO: It would be an optimization to know the texture has transparent or not

	_driver->drawPrimitive(roRDriverPrimitiveType_TriangleStrip, 0, 4, 0);
}


// ----------------------------------------------------------------------

void Canvas::beginBatch()
{
}

void Canvas::endBatch()
{
}


// ----------------------------------------------------------------------

float* Canvas::lockPixelData()
{
	return NULL;
}

void Canvas::unlockPixelData()
{
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
	vgDestroyPaint(g->handle);
	_allocator.deleteObj(g);
}


// ----------------------------------------------------------------------

void Canvas::stroke()
{
	const Mat4& m = _currentState.transform;
	float mat33[] = {
		m.m00, m.m10, m.m20,
		m.m01, m.m11, m.m21,
		m.m03, m.m13, m.m33,
	};

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
	color[3] *= _currentState.globalAlpha;

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
	const Mat4& m = _currentState.transform;
	float mat33[] = {
		m.m00, m.m10, m.m20,
		m.m01, m.m11, m.m21,
		m.m03, m.m13, m.m33,
	};

	vgLoadMatrix(mat33);
	vgDrawPath(_openvg->path, VG_FILL_PATH);
}

void Canvas::fillRect(float x, float y, float w, float h)
{
	vgClearPath(_openvg->pathSimpleShape, VG_PATH_CAPABILITY_ALL);
	vguRect(_openvg->pathSimpleShape, x, y, w, h);

	const Mat4& m = _currentState.transform;
	float mat33[] = {
		m.m00, m.m10, m.m20,
		m.m01, m.m11, m.m21,
		m.m03, m.m13, m.m33,
	};

	vgLoadMatrix(mat33);
	vgDrawPath(_openvg->pathSimpleShape, VG_FILL_PATH);
}

void Canvas::getFillColor(float* rgba)
{
	vgGetParameterfv(_openvg->fillPaint, VG_PAINT_COLOR, 4, rgba);
}

void Canvas::setFillColor(const float* rgba)
{
	float color[4];
	roMemcpy(color, rgba, sizeof(color));
	color[3] *= _currentState.globalAlpha;

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


// ----------------------------------------------------------------------

unsigned Canvas::width() const {
	return (unsigned)_targetWidth;
}

unsigned Canvas::height() const {
	return (unsigned)_targetHeight;
}

float Canvas::globalAlpha() const {
	return _currentState.globalAlpha;
}

void Canvas::setGlobalAlpha(float alpha)
{
	float sc[4];
	roMemcpy(sc, _currentState.strokeColor, sizeof(sc));

	float fc[4];
	roMemcpy(fc, _currentState.fillColor, sizeof(fc));

	sc[3] *= alpha;
	fc[3] *= alpha;

	vgSetParameterfv(_openvg->strokePaint, VG_PAINT_COLOR, 4, sc);
	vgSetParameterfv(_openvg->fillPaint, VG_PAINT_COLOR, 4, fc);

	_currentState.globalAlpha = alpha;
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
