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
#include "shDefs.h"
#include "shContext.h"
#include "shPath.h"
#include "shImage.h"
#include "shGeometry.h"
#include "shPaint.h"

#include "../roRenderDriver.h"
#include "../../base/roStringHash.h"

// Replacement for texture coordinate generation using texture matrix
// http://www.fernlightning.com/doku.php?id=randd:opengles
#define USE_TEXCOORD_GEN 0

//using namespace ro;

static roRDriverBlendState colorWrite_Disabled = {
	0, false,
	roRDriverBlendOp_Add, roRDriverBlendOp_Add,
	roRDriverBlendValue_Zero, roRDriverBlendValue_Zero,
	roRDriverBlendValue_Zero, roRDriverBlendValue_Zero,
	roRDriverColorWriteMask_DisableAll
};

static roRDriverBlendState blendState_Disabled = {
	0, false,
	roRDriverBlendOp_Add, roRDriverBlendOp_Add,
	roRDriverBlendValue_Zero, roRDriverBlendValue_Zero,	// Color src, dst
	roRDriverBlendValue_Zero, roRDriverBlendValue_Zero,	// Alpha src, dst
	roRDriverColorWriteMask_EnableAll
};

static roRDriverBlendState blendState_Src = {
	0, false,
	roRDriverBlendOp_Add, roRDriverBlendOp_Add,
	roRDriverBlendValue_One, roRDriverBlendValue_Zero,
	roRDriverBlendValue_One, roRDriverBlendValue_One,
	roRDriverColorWriteMask_EnableAll
};

static roRDriverBlendState blendState_SrcIn = {
	0, true,
	roRDriverBlendOp_Add, roRDriverBlendOp_Add,
	roRDriverBlendValue_DstAlpha, roRDriverBlendValue_Zero,
	roRDriverBlendValue_One, roRDriverBlendValue_One,
	roRDriverColorWriteMask_EnableAll
};

static roRDriverBlendState blendState_DstIn = {
	0, true,
	roRDriverBlendOp_Add, roRDriverBlendOp_Add,
	roRDriverBlendValue_Zero, roRDriverBlendValue_SrcAlpha,
	roRDriverBlendValue_One, roRDriverBlendValue_One,
	roRDriverColorWriteMask_EnableAll
};

static roRDriverBlendState blendState_SrcOutSh = {
	0, true,
	roRDriverBlendOp_Add, roRDriverBlendOp_Add,
	roRDriverBlendValue_InvDstAlpha, roRDriverBlendValue_Zero,
	roRDriverBlendValue_One, roRDriverBlendValue_One,
	roRDriverColorWriteMask_EnableAll
};

static roRDriverBlendState blendState_DstOutSh = {
	0, true,
	roRDriverBlendOp_Add, roRDriverBlendOp_Add,
	roRDriverBlendValue_Zero, roRDriverBlendValue_InvSrcAlpha,
	roRDriverBlendValue_One, roRDriverBlendValue_One,
	roRDriverColorWriteMask_EnableAll
};

static roRDriverBlendState blendState_SrcAtopSh = {
	0, true,
	roRDriverBlendOp_Add, roRDriverBlendOp_Add,
	roRDriverBlendValue_DstAlpha, roRDriverBlendValue_InvSrcAlpha,
	roRDriverBlendValue_One, roRDriverBlendValue_One,
	roRDriverColorWriteMask_EnableAll
};

static roRDriverBlendState blendState_DstAtopSh = {
	0, true,
	roRDriverBlendOp_Add, roRDriverBlendOp_Add,
	roRDriverBlendValue_InvDstAlpha, roRDriverBlendValue_SrcAlpha,
	roRDriverBlendValue_One, roRDriverBlendValue_One,
	roRDriverColorWriteMask_EnableAll
};

static roRDriverBlendState blendState_DstOver = {
	0, true,
	roRDriverBlendOp_Add, roRDriverBlendOp_Add,
	roRDriverBlendValue_InvDstAlpha, roRDriverBlendValue_DstAlpha,
	roRDriverBlendValue_One, roRDriverBlendValue_One,
	roRDriverColorWriteMask_EnableAll
};

static roRDriverBlendState blendState_SrcOver = {
	0, true,
	roRDriverBlendOp_Add, roRDriverBlendOp_Add,
	roRDriverBlendValue_SrcAlpha, roRDriverBlendValue_InvSrcAlpha,
	roRDriverBlendValue_One, roRDriverBlendValue_One,
	roRDriverColorWriteMask_EnableAll
};

static roRDriverDepthStencilState stencilState_disabled = {
	0, true, false,
	roRDriverDepthCompareFunc_Always,
	1, 1,
	{ roRDriverDepthCompareFunc_Equal, roRDriverStencilOp_Zero, roRDriverStencilOp_Zero, roRDriverStencilOp_Zero },
	{ roRDriverDepthCompareFunc_Equal, roRDriverStencilOp_Zero, roRDriverStencilOp_Zero, roRDriverStencilOp_Zero }
};

static roRDriverDepthStencilState stencilState_drawOdd = {
	0, true, true,
	roRDriverDepthCompareFunc_Always,
	1, 1,
	{ roRDriverDepthCompareFunc_Equal, roRDriverStencilOp_Zero, roRDriverStencilOp_Zero, roRDriverStencilOp_Zero },
	{ roRDriverDepthCompareFunc_Equal, roRDriverStencilOp_Zero, roRDriverStencilOp_Zero, roRDriverStencilOp_Zero }
};

static roRDriverDepthStencilState stencilState_drawNonZero = {
	0, true, true,
	roRDriverDepthCompareFunc_Always,
	0, 0xFF,
	{ roRDriverDepthCompareFunc_NotEqual, roRDriverStencilOp_Zero, roRDriverStencilOp_Zero, roRDriverStencilOp_Zero },
	{ roRDriverDepthCompareFunc_NotEqual, roRDriverStencilOp_Zero, roRDriverStencilOp_Zero, roRDriverStencilOp_Zero }
};

static roRDriverDepthStencilState stencilState_tesselateTo_OddEven = {
	0, true, true,
	roRDriverDepthCompareFunc_Always,
	0, 0,
	{ roRDriverDepthCompareFunc_Always, roRDriverStencilOp_Invert, roRDriverStencilOp_Invert, roRDriverStencilOp_Invert },
	{ roRDriverDepthCompareFunc_Always, roRDriverStencilOp_Invert, roRDriverStencilOp_Invert, roRDriverStencilOp_Invert }
};

static roRDriverDepthStencilState stencilState_tesselateTo_NonZero = {
	0, true, true,
	roRDriverDepthCompareFunc_Always,
	0, 0,
	{ roRDriverDepthCompareFunc_Always, roRDriverStencilOp_IncrWrap, roRDriverStencilOp_IncrWrap, roRDriverStencilOp_IncrWrap },
	{ roRDriverDepthCompareFunc_Always, roRDriverStencilOp_DecrWrap, roRDriverStencilOp_DecrWrap, roRDriverStencilOp_DecrWrap }
};

static roRDriverDepthStencilState stencilState_strokeTo = {
	0, true, true,
	roRDriverDepthCompareFunc_Always,
	1, 1,
	{ roRDriverDepthCompareFunc_NotEqual, roRDriverStencilOp_Keep, roRDriverStencilOp_Incr, roRDriverStencilOp_Incr },
	{ roRDriverDepthCompareFunc_NotEqual, roRDriverStencilOp_Keep, roRDriverStencilOp_Incr, roRDriverStencilOp_Incr }
};

static roRDriverDepthStencilState stencilState_quadTo = {
	0, true, true,
	roRDriverDepthCompareFunc_Always,
	1, 1,
	{ roRDriverDepthCompareFunc_Always, roRDriverStencilOp_Replace, roRDriverStencilOp_Replace, roRDriverStencilOp_Replace },
	{ roRDriverDepthCompareFunc_Always, roRDriverStencilOp_Replace, roRDriverStencilOp_Replace, roRDriverStencilOp_Replace }
};

void updateBlendingStateGL(VGContext *c, int alphaIsOne)
{
	// Most common drawing mode (SRC_OVER with alpha=1)
	// as well as SRC is optimized by turning OpenGL
	// blending off. In other cases its turned on.

	switch (c->blendMode)
	{
	case VG_BLEND_SRC:
		c->driver->setBlendState(&blendState_Src); break;
	case VG_BLEND_SRC_IN:
		c->driver->setBlendState(&blendState_SrcIn); break;
	case VG_BLEND_DST_IN:
		c->driver->setBlendState(&blendState_DstIn); break;
	case VG_BLEND_SRC_OUT_SH:
		c->driver->setBlendState(&blendState_SrcOutSh); break;
	case VG_BLEND_DST_OUT_SH:
		c->driver->setBlendState(&blendState_DstOutSh); break;
	case VG_BLEND_SRC_ATOP_SH:
		c->driver->setBlendState(&blendState_SrcAtopSh); break;
	case VG_BLEND_DST_ATOP_SH:
		c->driver->setBlendState(&blendState_DstAtopSh); break;
	case VG_BLEND_DST_OVER:
		c->driver->setBlendState(&blendState_DstOver); break;
	case VG_BLEND_SRC_OVER: default:
		if (alphaIsOne) c->driver->setBlendState(&blendState_Disabled);
		else c->driver->setBlendState(&blendState_SrcOver);
	};
}

/*-----------------------------------------------------------
 * Draws the triangles representing the stroke of a path.
 *-----------------------------------------------------------*/

static void shDrawStroke(VGContext* c, SHPath *p)
{
	roRDriverBuffer* vBuf = c->driver->newBuffer();
	c->strokeLayout[0].buffer = vBuf;
	c->strokeLayout[1].buffer = vBuf;
	roVerify(c->driver->initBuffer(vBuf, roRDriverBufferType_Vertex, roRDriverDataUsage_Stream, p->stroke.items, p->stroke.size * sizeof(SHVector2)));
	roVerify(c->driver->bindShaderInput(c->strokeLayout, roCountof(c->strokeLayout), NULL));
	c->driver->drawTriangle(0, p->stroke.size, 0);
	c->driver->deleteBuffer(vBuf);
}

/*-----------------------------------------------------------
 * Draws the subdivided vertices in the OpenGL mode given
 * (this could be VG_TRIANGLE_FAN or VG_LINE_STRIP).
 *-----------------------------------------------------------*/

static void shDrawVertices(VGContext* c, SHPath *p, roRDriverPrimitiveType primitiveType)
{
	int start = 0;
	int size = 0;

	roRDriverBuffer* vBuf = c->driver->newBuffer();
	c->shVertexLayout[0].buffer = vBuf;
	c->shVertexLayout[1].buffer = vBuf;
	roVerify(c->driver->initBuffer(vBuf, roRDriverBufferType_Vertex, roRDriverDataUsage_Stream, p->vertices.items, p->vertices.size * sizeof(SHVertex)));
	roVerify(c->driver->bindShaderInput(c->shVertexLayout, roCountof(c->shVertexLayout), NULL));

	// We separate vertex arrays by contours to properly handle the fill modes
	while (start < p->vertices.size) {
		size = p->vertices.items[start].flags;
		c->driver->drawPrimitive(primitiveType, start, size, 0);
		start += size;
	}

	c->driver->deleteBuffer(vBuf);
}

void shDrawQuad(VGContext* c, float* fourCorners)
{
	roVerify(c->driver->updateBuffer(c->quadBuffer, 0, fourCorners, sizeof(float) * 4 * 2));
	roVerify(c->driver->bindShaderInput(c->quadInputLayout, roCountof(c->quadInputLayout), NULL));
	c->driver->drawPrimitive(roRDriverPrimitiveType_TriangleStrip, 0, 4, 0);
}

static float whiteColor[4] = { 1, 1, 1, 1 };

static void setColor(VGContext* c, float* color)
{
	c->driver->updateBuffer(c->uBuffer, roOffsetof(UniformBuffer, UniformBuffer::color), color, sizeof(SHColor));
}

/*-------------------------------------------------------------
 * Draw a single quad that covers the bounding box of a path
 *-------------------------------------------------------------*/
/*
static void shDrawBoundBox(VGContext* c, SHPath *p, VGPaintMode mode)
{
	SHfloat K = 1.0f;
	if (mode == VG_STROKE_PATH)
		K = SH_CEIL(c->strokeMiterLimit * c->strokeLineWidth) + 1.0f;

	float corners[8] = {
		p->min.x-K, p->min.y-K,
		p->max.x+K, p->min.y-K,
		p->min.x-K, p->max.y+K,
		p->max.x+K, p->max.y+K,
	};

	shDrawQuad(c, corners);
}*/

/*--------------------------------------------------------------
 * Constructs & draws colored OpenGL primitives that cover the
 * given bounding box to represent the currently selected
 * stroke or fill paint
 *--------------------------------------------------------------*/

static void shDrawPaintMesh(VGContext *c, SHVector2 *min, SHVector2 *max,
                            VGPaintMode mode, GLenum texUnit)
{
	SHPaint *p;
	SHVector2 pmin, pmax;
	SHfloat K = 1.0f;

	// Pick the right paint
	if (mode == VG_FILL_PATH)
		p = (c->fillPaint ? c->fillPaint : &c->defaultPaint);
	else if (mode == VG_STROKE_PATH) {
		p = (c->strokePaint ? c->strokePaint : &c->defaultPaint);
		K = SH_CEIL(c->strokeMiterLimit * c->strokeLineWidth) + 1.0f;
	}
	else
		return;

	// We want to be sure to cover every pixel of this path so better
	// take a pixel more than leave some out (multisampling is tricky).
	SET2V(pmin, (*min)); SUB2(pmin, K,K);
	SET2V(pmax, (*max)); ADD2(pmax, K,K);

	// Construct appropriate OpenGL primitives so as
	// to fill the stencil mask with select paint

	switch (p->type) {
	case VG_PAINT_TYPE_LINEAR_GRADIENT:
		shDrawLinearGradientMesh(p, min, max, mode, texUnit);
		break;

	case VG_PAINT_TYPE_RADIAL_GRADIENT:
		shDrawRadialGradientMesh(p, min, max, mode, texUnit);
		break;

	case VG_PAINT_TYPE_PATTERN:
		if (p->pattern != VG_INVALID_HANDLE) {
			shDrawPatternMesh(p, min, max, mode, texUnit);
			break;
		}// else behave as a color paint

	case VG_PAINT_TYPE_COLOR: {
		float corners[8] = {
			pmin.x, pmin.y, pmax.x, pmin.y,
			pmin.x, pmax.y, pmax.x, pmax.y,
		};
		roVerify(c->driver->setUniformTexture(ro::stringHash("texGrad"), c->whiteTexture));
		setColor(c, &p->color.r);
		shDrawQuad(c, corners);
		} break;
	}
}

/*-----------------------------------------------------------
 * Tessellates / strokes the path and draws it according to
 * VGContext state.
 *-----------------------------------------------------------*/

VG_API_CALL void vgDrawPath(VGPath path, VGbitfield paintModes)
{
	SHPath *p;
	SHMatrix3x3 mi;
	SHPaint *fill, *stroke;

	VG_GETCONTEXT(VG_NO_RETVAL);

	VG_RETURN_ERR_IF(!shIsValidPath(context, path),
		VG_BAD_HANDLE_ERROR, VG_NO_RETVAL);

	VG_RETURN_ERR_IF(paintModes & (~(VG_STROKE_PATH | VG_FILL_PATH)),
		VG_ILLEGAL_ARGUMENT_ERROR, VG_NO_RETVAL);

	p = (SHPath*)path;

	// If user-to-surface matrix invertible tessellate in
	// surface space for better path resolution
	if (false && shInvertMatrix(&context->pathTransform, &mi)) {
		shFlattenPath(p, 1);
		shTransformVertices(&mi, p);
	} else
		shFlattenPath(p, 0);
	shFindBoundbox(p);

	// TODO: Turn antialiasing on/off
//	glEnable(GL_MULTISAMPLE);

	// Pick paint if available or default
	fill = (context->fillPaint ? context->fillPaint : &context->defaultPaint);
	stroke = (context->strokePaint ? context->strokePaint : &context->defaultPaint);

	// Apply the shaders
	roRDriverShader* shaders[] = { context->vShader, context->pShader };
	roVerify(context->driver->bindShaders(shaders, roCountof(shaders)));

	// Apply transformation
	shMatrixToGL(&context->pathTransform, context->viewMat.data);
	context->driver->updateBuffer(context->uBuffer, roOffsetof(UniformBuffer, UniformBuffer::viewMat), context->viewMat.data, sizeof(context->viewMat));

	if (paintModes & VG_FILL_PATH)
	{
		// Tessellate into stencil
		context->driver->setDepthStencilState(context->fillRule == VG_EVEN_ODD ?
			&stencilState_tesselateTo_OddEven : &stencilState_tesselateTo_NonZero);

		context->driver->setBlendState(&colorWrite_Disabled);
		shDrawVertices(context, p, roRDriverPrimitiveType_TriangleFan);

		// Setup blending
		updateBlendingStateGL(context,
			fill->type == VG_PAINT_TYPE_COLOR &&
			fill->color.a == 1.0f);

		// Draw paint where stencil odd
		context->driver->setDepthStencilState(context->fillRule == VG_EVEN_ODD ?
			&stencilState_drawOdd : &stencilState_drawNonZero);

		shDrawPaintMesh(context, &p->min, &p->max, VG_FILL_PATH, 0);

		// TODO: Clear stencil to zero if necessary
	}

	// TODO: Turn antialiasing on/off
//	glEnable(GL_MULTISAMPLE);

	if(!((paintModes & VG_STROKE_PATH) && context->strokeLineWidth > 0.0f)) {
		// Reset state
//		glDisable(GL_MULTISAMPLE);
		context->driver->setBlendState(&blendState_Disabled);
		context->driver->setDepthStencilState(&stencilState_disabled);

		VG_RETURN(VG_NO_RETVAL);
	}

	if (context->strokeLineWidth > 1.0f || stroke->type != VG_PAINT_TYPE_COLOR)
	{
		// Generate stroke triangles in path space
		shVector2ArrayClear(&p->stroke);
		shStrokePath(context, p);

		// Stroke into stencil
		context->driver->setDepthStencilState(&stencilState_strokeTo);
		context->driver->setBlendState(&colorWrite_Disabled);
		shDrawStroke(context, p);

		// Setup blending
		updateBlendingStateGL(context,
			stroke->type == VG_PAINT_TYPE_COLOR &&
			stroke->color.a == 1.0f);

		// Draw paint with stencil mask
		context->driver->setDepthStencilState(context->fillRule == VG_EVEN_ODD ?
			&stencilState_drawOdd : &stencilState_drawNonZero);

		shDrawPaintMesh(context, &p->min, &p->max, VG_STROKE_PATH, 0);

		// TODO: Clear stencil to zero if necessary

		// Reset state
		context->driver->setBlendState(&blendState_Disabled);
		context->driver->setDepthStencilState(&stencilState_disabled);
	}
	else
	{
		// Simulate thin stroke (without pattern) by alpha
		SHColor c = stroke->color;
		if (context->strokeLineWidth < 1.0f)
			c.a *= context->strokeLineWidth;

		setColor(context, &c.r);
		updateBlendingStateGL(context, c.a == 1.0f);

		// Draw contour as a line
		shDrawVertices(context, p, roRDriverPrimitiveType_LineStrip);
	}

	VG_RETURN(VG_NO_RETVAL);
}

VG_API_CALL void vgDrawImage(VGImage image)
{
/*	SHImage *i;
	SHfloat mgl[16];
	SHfloat texGenS[4] = {0,0,0,0};
	SHfloat texGenT[4] = {0,0,0,0};
	SHPaint *fill;
	SHVector2 min, max;

	VG_GETCONTEXT(VG_NO_RETVAL);

	VG_RETURN_ERR_IF(!shIsValidImage(context, image),
		VG_BAD_HANDLE_ERROR, VG_NO_RETVAL);

	// TODO: check if image is current render target

	// Apply image-user-to-surface transformation
	i = (SHImage*)image;
	shMatrixToGL(&context->imageTransform, mgl);
	glPushMatrix();
	glMultMatrixf(mgl);

	// Clamp to edge for proper filtering, modulate for multiply mode
	roRDriverTextureState state = {
		0,
		roRDriverTextureFilterMode_MinMagPoint,
		roRDriverTextureAddressMode_Edge,
		roRDriverTextureAddressMode_Edge,
		1
	};

	// Adjust antialiasing to settings
	if (context->imageQuality == VG_IMAGE_QUALITY_NONANTIALIASED) {
		state.filter = Driver::SamplerState::MIN_MAG_POINT;
//		glDisable(GL_MULTISAMPLE);
	}
	else {
		state.filter = Driver::SamplerState::MIN_MAG_LINEAR;
//		glEnable(GL_MULTISAMPLE);
	}
	context->driver->setTextureState(&state, 1, 0);
//	context->driver->setUniformTexture(stringHash("u_tex"), i->texture);
//	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	// Generate image texture coords automatically
	texGenS[0] = 1.0f / i->texwidth;
	texGenT[1] = 1.0f / i->texheight;

#if USE_TEXCOORD_GEN
	glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
	glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
	glTexGenfv(GL_S, GL_OBJECT_PLANE, texGenS);
	glTexGenfv(GL_T, GL_OBJECT_PLANE, texGenT);
	glEnable(GL_TEXTURE_GEN_S);
	glEnable(GL_TEXTURE_GEN_T);
#else
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glMatrixMode(GL_TEXTURE);
	GLfloat m[4][4] = {
		{ texGenS[0], texGenT[0], 0, 0 },
		{ texGenS[1], texGenT[1], 0, 0 },
		{ texGenS[2], texGenT[2], 1, 0 },
		{ texGenS[3], texGenT[3], 0, 1 }
	};
	glLoadMatrixf((GLfloat *)m);
	glMatrixMode(GL_MODELVIEW);
#endif

	// Pick fill paint
	fill = (context->fillPaint ? context->fillPaint : &context->defaultPaint);

	// Use paint color when multiplying with a color-paint
	if (context->imageMode == VG_DRAW_IMAGE_MULTIPLY &&
		fill->type == VG_PAINT_TYPE_COLOR)
		glColor4fv((GLfloat*)&fill->color);
	else glColor4f(1,1,1,1);


	// Check image drawing mode
	if (context->imageMode == VG_DRAW_IMAGE_MULTIPLY &&
		fill->type != VG_PAINT_TYPE_COLOR)
	{
		// Draw image quad into stencil
		Driver::setBlendEnable(false);
		glDisable(GL_TEXTURE_2D);
		context->driver->setDepthStencilState(&stencilState_quadTo);
		Driver::setColorWriteMask(Driver::BlendState::DisableAll);

		Render::Driver::drawQuad(
			0, 0,
			(float)i->width, 0,
			(float)i->width, (float)i->height,
			0, (float)i->height,
			0.0f,
			(rhuint8)255u, (rhuint8)255u, (rhuint8)255u, (rhuint8)255u
		);

		// Setup blending
		updateBlendingStateGL(context, 0);

		// Draw gradient mesh with stencil mask
		glEnable(GL_TEXTURE_2D);
		if (context->fillRule == VG_EVEN_ODD)
			context->driver->setDepthStencilState(&stencilState_drawOdd);
		else
			context->driver->setDepthStencilState(&stencilState_drawNonZero);

		SET2(min,0,0);
		SET2(max, (SHfloat)i->width, (SHfloat)i->height);
		if (fill->type == VG_PAINT_TYPE_RADIAL_GRADIENT) {
			shDrawRadialGradientMesh(fill, &min, &max, VG_FILL_PATH, 1);
		} else if (fill->type == VG_PAINT_TYPE_LINEAR_GRADIENT) {
			shDrawLinearGradientMesh(fill, &min, &max, VG_FILL_PATH, 1);
		} else if (fill->type == VG_PAINT_TYPE_PATTERN) {
			shDrawPatternMesh(fill, &min, &max, VG_FILL_PATH, GL_TEXTURE1); }

		glActiveTexture(GL_TEXTURE0);
		glDisable(GL_TEXTURE_2D);
		Driver::setStencilTestEnable(false);
	}
	else if (context->imageMode == VG_DRAW_IMAGE_STENCIL)
	{
	}
	else	// Either normal mode or multiplying with a color-paint
	{
		// Setup blending
		updateBlendingStateGL(context, 0);

		// Draw textured quad
		glEnable(GL_TEXTURE_2D);

		Render::Driver::drawQuad(
			0, 0,
			(float)i->width, 0,
			(float)i->width, (float)i->height,
			0, (float)i->height,
			0.0f,
			(rhuint8)255u, (rhuint8)255u, (rhuint8)255u, (rhuint8)255u
		);

		glDisable(GL_TEXTURE_2D);
	}

#if USE_TEXCOORD_GEN
	glDisable(GL_TEXTURE_GEN_S);
	glDisable(GL_TEXTURE_GEN_T);
#else
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
#endif

	glPopMatrix();

	VG_RETURN(VG_NO_RETVAL);*/
}
