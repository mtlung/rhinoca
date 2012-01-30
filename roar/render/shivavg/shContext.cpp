/*
 * Copyright (c) 2007 Ivan Leben
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library in the file COPYING;
 * if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "pch.h"

#define VG_API_EXPORT
#include "openvg.h"
#include "shContext.h"
#include <string.h>
#include <stdio.h>

#include "../../math/roMatrix.h"
#include "../../base/roStringHash.h"

using namespace ro;

/*-----------------------------------------------------
 * Simple functions to create a VG context instance
 * on top of an existing OpenGL context.
 * TODO: There is no mechanics yet to assure the OpenGL
 * context exists and to choose which context / window
 * to bind to.
 *-----------------------------------------------------*/

static VGContext *g_context = NULL;
static int g_initCount = 0;

VG_API_CALL VGboolean vgCreateContextSH(VGint width, VGint height, void* graphicDriverContext)
{
	++g_initCount;

	// return if already created
	if (g_context) return VG_TRUE;

	// create new context
	SH_NEWOBJ(VGContext, g_context);
	if (!g_context) return VG_FALSE;

	g_context->driverContext = (roRDriverContext*)graphicDriverContext;
	g_context->driver = g_context->driverContext->driver;

	g_context->viewMat.identity();

	vgResizeSurfaceSH(width, height);

	// create shaders
	static const char* vShaderSrc[] =
	{
		// GLSL
		"uniform constants { vec4 color; mat4 viewMat, projMat; };"
		"in vec2 position;"
		"in vec2 uv;"
		"out varying vec2 _uv;"
		"void main(void) {"
		"	vec4 pos = vec4(position, 0, 1);"
		"	pos = projMat * viewMat * pos;"
		"	pos.y = -pos.y;"
		"	gl_Position = pos;"
		"	_uv = uv;"
		"}",
	};

	static const char* pShaderSrc[] =
	{
		// GLSL
		"uniform constants { vec4 color; mat4 viewMat, projMat; };"
		"uniform sampler2D texGrad;"
		"in vec2 _uv;"
		"void main(void) {"
		"	gl_FragColor = color * texture2D(texGrad, _uv);"
		"}",
	};

	VGContext* c = g_context;
	roRDriver* d = g_context->driver;

	c->vShader = d->newShader();
	c->pShader = d->newShader();

	roVerify(d->initShader(c->vShader, roRDriverShaderType_Vertex, &vShaderSrc[0], 1));
	roVerify(d->initShader(c->pShader, roRDriverShaderType_Pixel, &pShaderSrc[0], 1));

	c->quadBuffer = d->newBuffer();
	c->vBuffer = d->newBuffer();
	c->uBuffer = d->newBuffer();

	// Vertex position, uv
	roVerify(d->initBuffer(c->quadBuffer, roRDriverBufferType_Vertex, roRDriverDataUsage_Stream, NULL, sizeof(float)*4*2));
	roVerify(d->initBuffer(c->uBuffer, roRDriverBufferType_Uniform, roRDriverDataUsage_Stream, NULL, sizeof(UniformBuffer)));

	// Init the uniform buffer
	float whiteColor[4] = { 1, 1, 1, 1 };
	d->updateBuffer(c->uBuffer, roOffsetof(UniformBuffer, UniformBuffer::color), whiteColor, sizeof(whiteColor));
	d->updateBuffer(c->uBuffer, roOffsetof(UniformBuffer, UniformBuffer::viewMat), (float*)mat4Identity.data, sizeof(c->viewMat));
	d->updateBuffer(c->uBuffer, roOffsetof(UniformBuffer, UniformBuffer::projMat), (float*)mat4Identity.data, sizeof(c->projMat));

	// Bind shader input layout
	{	// For use with primitive of 1 texture coordinate channel
		// {posx, posy, u, v}, {posx, posy, u, v}, ...
		const roRDriverShaderInput input[] = {
			{ c->vBuffer, c->vShader, "position", 0, 0, sizeof(float)*2, sizeof(float)*2*2 },
			{ c->vBuffer, c->vShader, "uv", 0, sizeof(float)*2, sizeof(float)*2, sizeof(float)*2*2 },
			{ c->uBuffer, c->vShader, "constants", 0, 0, 0, 0 },
			{ c->uBuffer, c->pShader, "constants", 0, 0, 0, 0 },
		};
		roAssert(roCountof(input) == roCountof(c->tex1VertexLayout));
		for(roSize i=0; i<roCountof(input); ++i)
			c->tex1VertexLayout[i] = input[i];
	}

	{	// For use with primitive without any texture
		// {posx, posy}, {posx, posy}, ...
		const roRDriverShaderInput input[] = {
			{ c->vBuffer, c->vShader, "position", 0, 0, sizeof(SHVertex), 0 },
			{ c->vBuffer, c->vShader, "uv", 0, 0, sizeof(SHVertex), 0 },	// We don't care about uv, so just use the position as uv
			{ c->uBuffer, c->vShader, "constants", 0, 0, 0, 0 },
			{ c->uBuffer, c->pShader, "constants", 0, 0, 0, 0 },
		};
		roAssert(roCountof(input) == roCountof(c->colorOnlyVertexLayout));
		for(roSize i=0; i<roCountof(input); ++i)
			c->colorOnlyVertexLayout[i] = input[i];
	}

	{	// For use with single quad of 1 texture coordinate channel
		// {posx, posy}, {posx, posy}, ... {u, v}, {u, v}, ...
		const roRDriverShaderInput input[] = {
			{ c->quadBuffer, c->vShader, "position", 0, 0, sizeof(float)*2, 0 },
			{ c->quadBuffer, c->vShader, "uv", 0, sizeof(float)*4*2, sizeof(float)*2, 0 },
			{ c->uBuffer, c->vShader, "constants", 0, 0, 0, 0 },
			{ c->uBuffer, c->pShader, "constants", 0, 0, 0, 0 },
		};
		roAssert(roCountof(input) == roCountof(c->quadInputLayout));
		for(roSize i=0; i<roCountof(input); ++i)
			c->quadInputLayout[i] = input[i];
	}

	roRDriverShader* shaders[] = { c->vShader, c->pShader };
	roVerify(c->driver->bindShaders(shaders, roCountof(shaders)));
	roVerify(c->driver->bindShaderInput(c->quadInputLayout, roCountof(c->quadInputLayout), NULL));

	// Create a white texture
	c->whiteTexture = d->newTexture();
	roUint8 color[4] = { 255, 255, 255, 255 };
	roVerify(d->initTexture(c->whiteTexture, 1, 1, roRDriverTextureFormat_RGBA, roRDriverTextureFlag_None));
	roVerify(d->commitTexture(c->whiteTexture, color, 0));

	// Set texture state
	roRDriverTextureState state[1] = {
		{	0,
			roRDriverTextureFilterMode_MinMagPoint,
			roRDriverTextureAddressMode_Edge,
			roRDriverTextureAddressMode_Edge,
			1
		},
	};
	d->setTextureState(state, roCountof(state), 0);

	// Assign the white texture as default
	roVerify(d->setUniformTexture(stringHash("texGrad"), c->whiteTexture));

	return VG_TRUE;
}

VG_API_CALL void vgResizeSurfaceSH(VGint width, VGint height)
{
	VG_GETCONTEXT(VG_NO_RETVAL);

	// update surface info
	context->surfaceWidth = width;
	context->surfaceHeight = height;

	// setup GL projection
	context->driver->setViewport(0, 0, (unsigned)width, (unsigned)height, 0, 1);
	context->projMat = makeOrthoMat4(0, (float)width, 0, (float)height, 0, 1);
	context->driver->updateBuffer(context->uBuffer, roOffsetof(UniformBuffer, UniformBuffer::projMat), context->projMat.data, sizeof(context->projMat));

	VG_RETURN(VG_NO_RETVAL);
}

VG_API_CALL void vgDestroyContextSH()
{
	--g_initCount;

	if(g_initCount) return;

	// return if already released
	if (!g_context) return;

	// delete context object
	SH_DELETEOBJ(VGContext, g_context);
	g_context = NULL;
}

VGContext* shGetContext()
{
	SH_ASSERT(g_context);
	return g_context;
}

/*-----------------------------------------------------
 * VGContext constructor
 *-----------------------------------------------------*/

void VGContext_ctor(VGContext *c)
{
	// Surface info
	c->surfaceWidth = 0;
	c->surfaceHeight = 0;

	// GetString info
	strncpy(c->vendor, "Ivan Leben", sizeof(c->vendor));
	strncpy(c->renderer, "ShivaVG 0.1.0", sizeof(c->renderer));
	strncpy(c->version, "1.0", sizeof(c->version));
	strncpy(c->extensions, "", sizeof(c->extensions));

	// Mode settings
	c->matrixMode = VG_MATRIX_PATH_USER_TO_SURFACE;
	c->fillRule = VG_EVEN_ODD;
	c->imageQuality = VG_IMAGE_QUALITY_FASTER;
	c->renderingQuality = VG_RENDERING_QUALITY_BETTER;
	c->blendMode = VG_BLEND_SRC_OVER;
	c->imageMode = VG_DRAW_IMAGE_NORMAL;

	// Scissor rectangles
	SH_INITOBJ(SHRectArray, c->scissor);
	c->scissoring = VG_FALSE;
	c->masking = VG_FALSE;

	// Stroke parameters
	c->strokeLineWidth = 1.0f;
	c->strokeCapStyle = VG_CAP_BUTT;
	c->strokeJoinStyle = VG_JOIN_MITER;
	c->strokeMiterLimit = 4.0f;
	c->strokeDashPhase = 0.0f;
	c->strokeDashPhaseReset = VG_FALSE;
	SH_INITOBJ(SHFloatArray, c->strokeDashPattern);

	// Edge fill color for vgConvolve and pattern paint
	CSET(c->tileFillColor, 0,0,0,0);

	// Color for vgClear
	CSET(c->clearColor, 0,0,0,0);

	// Color components layout inside pixel
	c->pixelLayout = VG_PIXEL_LAYOUT_UNKNOWN;

	// Source format for image filters
	c->filterFormatLinear = VG_FALSE;
	c->filterFormatPremultiplied = VG_FALSE;
	c->filterChannelMask = VG_RED|VG_GREEN|VG_BLUE|VG_ALPHA;

	// Matrices
	SH_INITOBJ(SHMatrix3x3, c->pathTransform);
	SH_INITOBJ(SHMatrix3x3, c->imageTransform);
	SH_INITOBJ(SHMatrix3x3, c->fillTransform);
	SH_INITOBJ(SHMatrix3x3, c->strokeTransform);

	// Paints
	c->fillPaint = NULL;
	c->strokePaint = NULL;
	SH_INITOBJ(SHPaint, c->defaultPaint);

	// Error
	c->error = VG_NO_ERROR;

	// Resources
	SH_INITOBJ(SHPathArray, c->paths);
	SH_INITOBJ(SHPaintArray, c->paints);
	SH_INITOBJ(SHImageArray, c->images);

	// Render driver
	c->driver = NULL;
	c->driverContext = NULL;
	c->quadBuffer = NULL;
	c->vBuffer = NULL;
	c->uBuffer = NULL;
	c->vShader = NULL;
	c->pShader = NULL;
	c->whiteTexture = NULL;
}

/*-----------------------------------------------------
 * VGContext constructor
 *-----------------------------------------------------*/

void VGContext_dtor(VGContext *c)
{
	int i;

	SH_DEINITOBJ(SHRectArray, c->scissor);
	SH_DEINITOBJ(SHFloatArray, c->strokeDashPattern);

	SH_DEINITOBJ(SHPaint, c->defaultPaint);

	// Destroy resources
	for (i=0; i<c->paths.size; ++i)
		SH_DELETEOBJ(SHPath, c->paths.items[i]);

	for (i=0; i<c->paints.size; ++i)
		SH_DELETEOBJ(SHPaint, c->paints.items[i]);

	for (i=0; i<c->images.size; ++i)
		SH_DELETEOBJ(SHImage, c->images.items[i]);

	SH_DEINITOBJ(SHPathArray, c->paths);
	SH_DEINITOBJ(SHPaintArray, c->paints);
	SH_DEINITOBJ(SHImageArray, c->images);

	c->driver->deleteBuffer(c->quadBuffer);
	c->driver->deleteBuffer(c->vBuffer);
	c->driver->deleteBuffer(c->uBuffer);
	c->driver->deleteShader(c->vShader);
	c->driver->deleteShader(c->pShader);
	c->driver->deleteTexture(c->whiteTexture);
}

/*--------------------------------------------------
 * Tries to find resources in this context
 *--------------------------------------------------*/

SHint shIsValidPath(VGContext *c, VGHandle h)
{
	int index = shPathArrayFind(&c->paths, (SHPath*)h);
	return (index == -1) ? 0 : 1;
}

SHint shIsValidPaint(VGContext *c, VGHandle h)
{
	int index = shPaintArrayFind(&c->paints, (SHPaint*)h);
	return (index == -1) ? 0 : 1;
}

SHint shIsValidImage(VGContext *c, VGHandle h)
{
	int index = shImageArrayFind(&c->images, (SHImage*)h);
	return (index == -1) ? 0 : 1;
}

/*--------------------------------------------------
 * Tries to find a resources in this context and
 * return its type or invalid flag.
 *--------------------------------------------------*/

SHResourceType shGetResourceType(VGContext *c, VGHandle h)
{
	if (shIsValidPath(c, h))
		return SH_RESOURCE_PATH;

	else if (shIsValidPaint(c, h))
		return SH_RESOURCE_PAINT;

	else if (shIsValidImage(c, h))
		return SH_RESOURCE_IMAGE;

	else
		return SH_RESOURCE_INVALID;
}

/*-----------------------------------------------------
 * Sets the specified error on the given context if
 * there is no pending error yet
 *-----------------------------------------------------*/

void shSetError(VGContext *c, VGErrorCode e)
{
	if (c->error == VG_NO_ERROR)
		c->error = e;
}

/*--------------------------------------------------
 * Returns the oldest error pending on the current
 * context and clears its error code
 *--------------------------------------------------*/

VG_API_CALL VGErrorCode vgGetError(void)
{
	VGErrorCode error;
	VG_GETCONTEXT(VG_NO_CONTEXT_ERROR);
	error = context->error;
	context->error = VG_NO_ERROR;
	VG_RETURN(error);
}

VG_API_CALL void vgFlush(void)
{
	VG_GETCONTEXT(VG_NO_RETVAL);
//	glFlush();
	VG_RETURN(VG_NO_RETVAL);
}

VG_API_CALL void vgFinish(void)
{
	VG_GETCONTEXT(VG_NO_RETVAL);
//	glFinish();
	VG_RETURN(VG_NO_RETVAL);
}

VG_API_CALL void vgMask(VGImage mask, VGMaskOperation operation,
                        VGint x, VGint y, VGint width, VGint height)
{
}

static roRDriverRasterizerState _useScissor = { 0, true };
static roRDriverRasterizerState _noScissor = { 0, false };

VG_API_CALL void vgClear(VGint x, VGint y, VGint width, VGint height)
{
  VG_GETCONTEXT(VG_NO_RETVAL);

  // Clip to window
  if (x < 0) x = 0;
  if (y < 0) y = 0;
  if (width > context->surfaceWidth) width = context->surfaceWidth;
  if (height > context->surfaceHeight) height = context->surfaceHeight;

  // Check if scissoring needed
  if(x > 0 || y > 0 ||
    width < context->surfaceWidth ||
    height < context->surfaceHeight)
  {
    context->driver->setRasterizerState(&_useScissor);
    context->driver->setScissorRect(x, context->surfaceHeight - height - y, width, height);
  }

  // Clear GL color buffer
  /* TODO: what about stencil and depth? when do we clear that?
     we would need some kind of special "begin" function at
     beginning of each drawing or clear the planes prior to each
     drawing where it takes places */
  context->driver->clearColor(
    context->clearColor.r,
    context->clearColor.g,
    context->clearColor.b,
    context->clearColor.a);

  context->driver->clearDepth(0);
  context->driver->clearStencil(0);

  context->driver->setRasterizerState(&_noScissor);

  VG_RETURN(VG_NO_RETVAL);
}

/*-----------------------------------------------------------
 * Returns the matrix currently selected via VG_MATRIX_MODE
 *-----------------------------------------------------------*/

SHMatrix3x3* shCurrentMatrix(VGContext *c)
{
	switch(c->matrixMode) {
	case VG_MATRIX_PATH_USER_TO_SURFACE:
		return &c->pathTransform;
	case VG_MATRIX_IMAGE_USER_TO_SURFACE:
		return &c->imageTransform;
	case VG_MATRIX_FILL_PAINT_TO_USER:
		return &c->fillTransform;
	default:
		return &c->strokeTransform;
	}
}

/*--------------------------------------
 * Sets the current matrix to identity
 *--------------------------------------*/

VG_API_CALL void vgLoadIdentity(void)
{
	SHMatrix3x3 *m;
	VG_GETCONTEXT(VG_NO_RETVAL);

	m = shCurrentMatrix(context);
	IDMAT((*m));

	VG_RETURN(VG_NO_RETVAL);
}

/*-------------------------------------------------------------
 * Loads values into the current matrix from the given array.
 * Matrix affinity is preserved if an affine matrix is loaded.
 *-------------------------------------------------------------*/

VG_API_CALL void vgLoadMatrix(const VGfloat * mm)
{
	SHMatrix3x3 *m;
	VG_GETCONTEXT(VG_NO_RETVAL);

	VG_RETURN_ERR_IF(!mm, VG_ILLEGAL_ARGUMENT_ERROR, VG_NO_RETVAL);
	// TODO: check matrix array alignment

	m = shCurrentMatrix(context);

	if (context->matrixMode == VG_MATRIX_IMAGE_USER_TO_SURFACE) {
		SETMAT((*m),
			mm[0], mm[3], mm[6],
			mm[1], mm[4], mm[7],
			mm[2], mm[5], mm[8]);
	}
	else {

		SETMAT((*m),
			mm[0], mm[3], mm[6],
			mm[1], mm[4], mm[7],
			0.0f,  0.0f,  1.0f);
	}

	VG_RETURN(VG_NO_RETVAL);
}

/*---------------------------------------------------------------
 * Outputs the values of the current matrix into the given array
 *---------------------------------------------------------------*/

VG_API_CALL void vgGetMatrix(VGfloat * mm)
{
	SHMatrix3x3 *m; int i,j,k=0;
	VG_GETCONTEXT(VG_NO_RETVAL);

	VG_RETURN_ERR_IF(!mm, VG_ILLEGAL_ARGUMENT_ERROR, VG_NO_RETVAL);
	// TODO: check matrix array alignment

	m = shCurrentMatrix(context);

	for (i=0; i<3; ++i)
		for (j=0; j<3; ++j)
			mm[k++] = m->m[j][i];

	VG_RETURN(VG_NO_RETVAL);
}

/*-------------------------------------------------------------
 * Right-multiplies the current matrix with the one specified
 * in the given array. Matrix affinity is preserved if an
 * affine matrix is begin multiplied.
 *-------------------------------------------------------------*/

VG_API_CALL void vgMultMatrix(const VGfloat * mm)
{
	SHMatrix3x3 *m, mul, temp;
	VG_GETCONTEXT(VG_NO_RETVAL);

	VG_RETURN_ERR_IF(!mm, VG_ILLEGAL_ARGUMENT_ERROR, VG_NO_RETVAL);
	// TODO: check matrix array alignment

	m = shCurrentMatrix(context);

	if (context->matrixMode == VG_MATRIX_IMAGE_USER_TO_SURFACE) {
		SETMAT(mul,
			mm[0], mm[3], mm[6],
			mm[1], mm[4], mm[7],
			mm[2], mm[5], mm[8]);
	}
	else {

		SETMAT(mul,
			mm[0], mm[3], mm[6],
			mm[1], mm[4], mm[7],
			0.0f,  0.0f,  1.0f);
	}

	MULMATMAT((*m), mul, temp);
	SETMATMAT((*m), temp);

	VG_RETURN(VG_NO_RETVAL);
}

VG_API_CALL void vgTranslate(VGfloat tx, VGfloat ty)
{
	SHMatrix3x3 *m;
	VG_GETCONTEXT(VG_NO_RETVAL);

	m = shCurrentMatrix(context);
	TRANSLATEMATR((*m), tx, ty);

	VG_RETURN(VG_NO_RETVAL);
}

VG_API_CALL void vgScale(VGfloat sx, VGfloat sy)
{
	SHMatrix3x3 *m;
	VG_GETCONTEXT(VG_NO_RETVAL);

	m = shCurrentMatrix(context);
	SCALEMATR((*m), sx, sy);

	VG_RETURN(VG_NO_RETVAL);
}

VG_API_CALL void vgShear(VGfloat shx, VGfloat shy)
{
	SHMatrix3x3 *m;
	VG_GETCONTEXT(VG_NO_RETVAL);

	m = shCurrentMatrix(context);
	SHEARMATR((*m), shx, shy);

	VG_RETURN(VG_NO_RETVAL);
}

VG_API_CALL void vgRotate(VGfloat angle)
{
	SHfloat a;
	SHMatrix3x3 *m;
	VG_GETCONTEXT(VG_NO_RETVAL);

	a = SH_DEG2RAD(angle);
	m = shCurrentMatrix(context);
	ROTATEMATR((*m), a);

	VG_RETURN(VG_NO_RETVAL);
}

VG_API_CALL VGHardwareQueryResult vgHardwareQuery(VGHardwareQueryType key,
												  VGint setting)
{
	return VG_HARDWARE_UNACCELERATED;
}
