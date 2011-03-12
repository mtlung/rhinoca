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
#include "../vg/openvg.h"
#include "shDefs.h"
#include "shExtensions.h"
#include "shContext.h"
#include "shPath.h"
#include "shImage.h"
#include "shGeometry.h"
#include "shPaint.h"

#include "../driver.h"

void shPremultiplyFramebuffer()
{
  /* Multiply target color with its own alpha */
  glBlendFunc(GL_ZERO, GL_DST_ALPHA);
}

void shUnpremultiplyFramebuffer()
{
  /* TODO: hmmmm..... any idea? */
}

using namespace Render;

static const Driver::BlendState blendState_Disabled = {
	false,
	Driver::BlendState::Add, Driver::BlendState::Add,
	Driver::BlendState::Zero, Driver::BlendState::Zero,	// Color src, dst
	Driver::BlendState::Zero, Driver::BlendState::Zero,	// Alpha src, dst
	Driver::BlendState::EnableAll
};

static const Driver::BlendState blendState_Src = {
	false,
	Driver::BlendState::Add, Driver::BlendState::Add,
	Driver::BlendState::One, Driver::BlendState::Zero,
	Driver::BlendState::One, Driver::BlendState::One,
	Driver::BlendState::EnableAll
};

static const Driver::BlendState blendState_SrcIn = {
	true,
	Driver::BlendState::Add, Driver::BlendState::Add,
	Driver::BlendState::DstAlpha, Driver::BlendState::Zero,
	Driver::BlendState::One, Driver::BlendState::One,
	Driver::BlendState::EnableAll
};

static const Driver::BlendState blendState_DstIn = {
	true,
	Driver::BlendState::Add, Driver::BlendState::Add,
	Driver::BlendState::Zero, Driver::BlendState::SrcAlpha,
	Driver::BlendState::One, Driver::BlendState::One,
	Driver::BlendState::EnableAll
};

static const Driver::BlendState blendState_SrcOutSh = {
	true,
	Driver::BlendState::Add, Driver::BlendState::Add,
	Driver::BlendState::InvDstAlpha, Driver::BlendState::Zero,
	Driver::BlendState::One, Driver::BlendState::One,
	Driver::BlendState::EnableAll
};

static const Driver::BlendState blendState_DstOutSh = {
	true,
	Driver::BlendState::Add, Driver::BlendState::Add,
	Driver::BlendState::Zero, Driver::BlendState::InvSrcAlpha,
	Driver::BlendState::One, Driver::BlendState::One,
	Driver::BlendState::EnableAll
};

static const Driver::BlendState blendState_SrcAtopSh = {
	true,
	Driver::BlendState::Add, Driver::BlendState::Add,
	Driver::BlendState::DstAlpha, Driver::BlendState::InvSrcAlpha,
	Driver::BlendState::One, Driver::BlendState::One,
	Driver::BlendState::EnableAll
};

static const Driver::BlendState blendState_DstAtopSh = {
	true,
	Driver::BlendState::Add, Driver::BlendState::Add,
	Driver::BlendState::InvDstAlpha, Driver::BlendState::SrcAlpha,
	Driver::BlendState::One, Driver::BlendState::One,
	Driver::BlendState::EnableAll
};

static const Driver::BlendState blendState_DstOver = {
	true,
	Driver::BlendState::Add, Driver::BlendState::Add,
	Driver::BlendState::InvDstAlpha, Driver::BlendState::DstAlpha,
	Driver::BlendState::One, Driver::BlendState::One,
	Driver::BlendState::EnableAll
};

static const Driver::BlendState blendState_SrcOver = {
	true,
	Driver::BlendState::Add, Driver::BlendState::Add,
	Driver::BlendState::SrcAlpha, Driver::BlendState::InvSrcAlpha,
	Driver::BlendState::One, Driver::BlendState::One,
	Driver::BlendState::EnableAll
};

static const Driver::DepthStencilState stencilState_drawOdd = Driver::DepthStencilState(
	true, Driver::DepthStencilState::Always,
	true, 
	Driver::DepthStencilState::StencilState(
		1, 1, Driver::DepthStencilState::Equal,
		Driver::DepthStencilState::StencilState::Zero
	)
);

static const Driver::DepthStencilState stencilState_drawNonZero = Driver::DepthStencilState(
	true, Driver::DepthStencilState::Always,
	true, 
	Driver::DepthStencilState::StencilState(
		0, 0xFF, Driver::DepthStencilState::NotEqual,
		Driver::DepthStencilState::StencilState::Zero
	)
);

static const Driver::DepthStencilState stencilState_tesselateTo_OddEven = Driver::DepthStencilState(
	true, Driver::DepthStencilState::Always,
	true,
	Driver::DepthStencilState::StencilState(
		0, 0, Driver::DepthStencilState::Always,
		Driver::DepthStencilState::StencilState::Invert
	)
);

static const Driver::DepthStencilState stencilState_tesselateTo_NonZero = Driver::DepthStencilState(
	true, Driver::DepthStencilState::Always,
	true,
	Driver::DepthStencilState::StencilState(
		0, 0, Driver::DepthStencilState::Always,
		Driver::DepthStencilState::StencilState::IncrWrap
	),
	Driver::DepthStencilState::StencilState(
		0, 0, Driver::DepthStencilState::Always,
		Driver::DepthStencilState::StencilState::DecrWrap
	)
);

static const Driver::DepthStencilState stencilState_strokeTo = Driver::DepthStencilState(
	true, Driver::DepthStencilState::Always,
	true,
	Driver::DepthStencilState::StencilState(
		1, 1, Driver::DepthStencilState::NotEqual,
		Driver::DepthStencilState::StencilState::Keep,
		Driver::DepthStencilState::StencilState::Incr,
		Driver::DepthStencilState::StencilState::Incr
	)
);

static const Driver::DepthStencilState stencilState_quadTo = Driver::DepthStencilState(
	true, Driver::DepthStencilState::Always,
	true,
	Driver::DepthStencilState::StencilState(
		1, 1, Driver::DepthStencilState::Always,
		Driver::DepthStencilState::StencilState::Replace
	)
);

void updateBlendingStateGL(VGContext *c, int alphaIsOne)
{
  /* Most common drawing mode (SRC_OVER with alpha=1)
     as well as SRC is optimized by turning OpenGL
     blending off. In other cases its turned on. */
  
  switch (c->blendMode)
  {
  case VG_BLEND_SRC:
    Driver::setBlendState(blendState_Src); break;
  case VG_BLEND_SRC_IN:
    Driver::setBlendState(blendState_SrcIn); break;
  case VG_BLEND_DST_IN:
    Driver::setBlendState(blendState_DstIn); break;
  case VG_BLEND_SRC_OUT_SH:
    Driver::setBlendState(blendState_SrcOutSh); break;
  case VG_BLEND_DST_OUT_SH:
    Driver::setBlendState(blendState_DstOutSh); break;
  case VG_BLEND_SRC_ATOP_SH:
    Driver::setBlendState(blendState_SrcAtopSh); break;
  case VG_BLEND_DST_ATOP_SH:
    Driver::setBlendState(blendState_DstAtopSh); break;
  case VG_BLEND_DST_OVER:
    Driver::setBlendState(blendState_DstOver); break;
  case VG_BLEND_SRC_OVER: default:
    if (alphaIsOne) Driver::setBlendState(blendState_Disabled);
    else Driver::setBlendState(blendState_SrcOver);
  };
}

/*-----------------------------------------------------------
 * Draws the triangles representing the stroke of a path.
 *-----------------------------------------------------------*/

static void shDrawStroke(SHPath *p)
{
  glEnableClientState(GL_VERTEX_ARRAY);
  glVertexPointer(2, GL_FLOAT, 0, p->stroke.items);
  glDrawArrays(GL_TRIANGLES, 0, p->stroke.size);
//  glDisableClientState(GL_VERTEX_ARRAY);
}

/*-----------------------------------------------------------
 * Draws the subdivided vertices in the OpenGL mode given
 * (this could be VG_TRIANGLE_FAN or VG_LINE_STRIP).
 *-----------------------------------------------------------*/

static void shDrawVertices(SHPath *p, GLenum mode)
{
  int start = 0;
  int size = 0;
  
  /* We separate vertex arrays by contours to properly
     handle the fill modes */
  glEnableClientState(GL_VERTEX_ARRAY);
  glVertexPointer(2, GL_FLOAT, sizeof(SHVertex), p->vertices.items);
  
  while (start < p->vertices.size) {
    size = p->vertices.items[start].flags;
    glDrawArrays(mode, start, size);
    start += size;
  }
  
//  glDisableClientState(GL_VERTEX_ARRAY);
}

/*-------------------------------------------------------------
 * Draw a single quad that covers the bounding box of a path
 *-------------------------------------------------------------*/

static void shDrawBoundBox(VGContext *c, SHPath *p, VGPaintMode mode)
{
  SHfloat K = 1.0f;
  if (mode == VG_STROKE_PATH)
    K = SH_CEIL(c->strokeMiterLimit * c->strokeLineWidth) + 1.0f;

  Render::Driver::drawQuad(
    p->min.x-K, p->min.y-K,
    p->max.x+K, p->min.y-K,
    p->max.x+K, p->max.y+K,
    p->min.x-K, p->max.y+K,
    0.0f,
    (rhuint8)255u, (rhuint8)255u, (rhuint8)255u, (rhuint8)255u
  );
}

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
  
  /* Pick the right paint */
  if (mode == VG_FILL_PATH) {
    p = (c->fillPaint ? c->fillPaint : &c->defaultPaint);
  }else if (mode == VG_STROKE_PATH) {
    p = (c->strokePaint ? c->strokePaint : &c->defaultPaint);
    K = SH_CEIL(c->strokeMiterLimit * c->strokeLineWidth) + 1.0f;
  }
  
  /* We want to be sure to cover every pixel of this path so better
     take a pixel more than leave some out (multisampling is tricky). */
  SET2V(pmin, (*min)); SUB2(pmin, K,K);
  SET2V(pmax, (*max)); ADD2(pmax, K,K);

  /* Construct appropriate OpenGL primitives so as
     to fill the stencil mask with select paint */

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
    }/* else behave as a color paint */
  
  case VG_PAINT_TYPE_COLOR:
    Render::Driver::drawQuad(
      pmin.x, pmin.y, pmax.x, pmin.y,
      pmax.x, pmax.y, pmin.x, pmax.y,
      0.0f,
      p->color.r, p->color.g, p->color.b, p->color.a
    );
    break;
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
  SHfloat mgl[16];
  SHPaint *fill, *stroke;
  
  VG_GETCONTEXT(VG_NO_RETVAL);
  
  VG_RETURN_ERR_IF(!shIsValidPath(context, path),
                   VG_BAD_HANDLE_ERROR, VG_NO_RETVAL);
  
  VG_RETURN_ERR_IF(paintModes & (~(VG_STROKE_PATH | VG_FILL_PATH)),
                   VG_ILLEGAL_ARGUMENT_ERROR, VG_NO_RETVAL);
  
  p = (SHPath*)path;
  
  /* If user-to-surface matrix invertible tessellate in
     surface space for better path resolution */
  if (shInvertMatrix(&context->pathTransform, &mi)) {
    shFlattenPath(p, 1);
    shTransformVertices(&mi, p);
  }else shFlattenPath(p, 0);
  shFindBoundbox(p);
  
  /* TODO: Turn antialiasing on/off */
  glEnable(GL_MULTISAMPLE);
  
  /* Pick paint if available or default*/
  fill = (context->fillPaint ? context->fillPaint : &context->defaultPaint);
  stroke = (context->strokePaint ? context->strokePaint : &context->defaultPaint);
  
  /* Apply transformation */
  shMatrixToGL(&context->pathTransform, mgl);
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glMultMatrixf(mgl);
  
  if (paintModes & VG_FILL_PATH) {
    
    /* Tesselate into stencil */
    if (context->fillRule == VG_EVEN_ODD)
        Driver::setDepthStencilState(stencilState_tesselateTo_OddEven);
	else
		Driver::setDepthStencilState(stencilState_tesselateTo_NonZero);
    Driver::setColorWriteMask(Driver::BlendState::DisableAll);
    shDrawVertices(p, GL_TRIANGLE_FAN);
    
    /* Setup blending */
    updateBlendingStateGL(context,
                          fill->type == VG_PAINT_TYPE_COLOR &&
                          fill->color.a == 1.0f);
    
    /* Draw paint where stencil odd */
	if (context->fillRule == VG_EVEN_ODD)
        Driver::setDepthStencilState(stencilState_drawOdd);
	else
		Driver::setDepthStencilState(stencilState_drawNonZero);
    shDrawPaintMesh(context, &p->min, &p->max, VG_FILL_PATH, GL_TEXTURE0);

    /* Clear stencil for sure */
    /* TODO: Is there any way to do this safely along
       with the paint generation pass?? */
    Driver::setBlendEnable(false);
    glDisable(GL_MULTISAMPLE);
    Driver::setColorWriteMask(Driver::BlendState::DisableAll);
    shDrawBoundBox(context, p, VG_FILL_PATH);
    
    /* Reset state */
    Driver::setColorWriteMask(Driver::BlendState::EnableAll);
    Driver::setStencilTestEnable(false);
    Driver::setBlendEnable(false);
  }
  
  /* TODO: Turn antialiasing on/off */
  glEnable(GL_MULTISAMPLE);
  
  if ((paintModes & VG_STROKE_PATH) &&
      context->strokeLineWidth > 0.0f) {
    
    if (1) {/*context->strokeLineWidth > 1.0f) {*/

      /* Generate stroke triangles in path space */
      shVector2ArrayClear(&p->stroke);
      shStrokePath(context, p);

      /* Stroke into stencil */
      Driver::setDepthStencilState(stencilState_strokeTo);
      Driver::setColorWriteMask(Driver::BlendState::DisableAll);
      shDrawStroke(p);

      /* Setup blending */
      updateBlendingStateGL(context,
                            stroke->type == VG_PAINT_TYPE_COLOR &&
                            stroke->color.a == 1.0f);

      /* Draw paint where stencil odd */
	  if (context->fillRule == VG_EVEN_ODD)
		  Driver::setDepthStencilState(stencilState_drawOdd);
      else
		  Driver::setDepthStencilState(stencilState_drawNonZero);
      shDrawPaintMesh(context, &p->min, &p->max, VG_STROKE_PATH, GL_TEXTURE0);
      
      /* Clear stencil for sure */
	  Driver::setBlendEnable(false);
      glDisable(GL_MULTISAMPLE);
      Driver::setColorWriteMask(Driver::BlendState::DisableAll);
      shDrawBoundBox(context, p, VG_STROKE_PATH);
      
      /* Reset state */
      Driver::setColorWriteMask(Driver::BlendState::EnableAll);
      Driver::setStencilTestEnable(false);
	  Driver::setBlendEnable(false);
      
    }else{
      
      /* Simulate thin stroke by alpha */
      SHColor c = stroke->color;
      if (context->strokeLineWidth < 1.0f)
        c.a *= context->strokeLineWidth;
      
      /* Draw contour as a line */
      glDisable(GL_MULTISAMPLE);
      Driver::setBlendEnable(true);
      glEnable(GL_LINE_SMOOTH);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glColor4fv((GLfloat*)&c);
      shDrawVertices(p, GL_LINE_STRIP);

      Driver::setBlendEnable(false);
      glDisable(GL_LINE_SMOOTH);
    }
  }
  
  
  glPopMatrix();
  
  VG_RETURN(VG_NO_RETVAL);
}

VG_API_CALL void vgDrawImage(VGImage image)
{
  SHImage *i;
  SHfloat mgl[16];
  SHfloat texGenS[4] = {0,0,0,0};
  SHfloat texGenT[4] = {0,0,0,0};
  SHPaint *fill;
  SHVector2 min, max;
  
  VG_GETCONTEXT(VG_NO_RETVAL);
  
  VG_RETURN_ERR_IF(!shIsValidImage(context, image),
                   VG_BAD_HANDLE_ERROR, VG_NO_RETVAL);
  
  /* TODO: check if image is current render target */
  
  /* Apply image-user-to-surface transformation */
  i = (SHImage*)image;
  shMatrixToGL(&context->imageTransform, mgl);
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glMultMatrixf(mgl);
  
  /* Clamp to edge for proper filtering, modulate for multiply mode */
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, i->texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
  
  /* Adjust antialiasing to settings */
  if (context->imageQuality == VG_IMAGE_QUALITY_NONANTIALIASED) {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glDisable(GL_MULTISAMPLE);
  }else{
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glEnable(GL_MULTISAMPLE);
  }
  
  /* Generate image texture coords automatically */
  texGenS[0] = 1.0f / i->texwidth;
  texGenT[1] = 1.0f / i->texheight;
  glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
  glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
  glTexGenfv(GL_S, GL_OBJECT_PLANE, texGenS);
  glTexGenfv(GL_T, GL_OBJECT_PLANE, texGenT);
  glEnable(GL_TEXTURE_GEN_S);
  glEnable(GL_TEXTURE_GEN_T);
  
  /* Pick fill paint */
  fill = (context->fillPaint ? context->fillPaint : &context->defaultPaint);
  
  /* Use paint color when multiplying with a color-paint */
  if (context->imageMode == VG_DRAW_IMAGE_MULTIPLY &&
      fill->type == VG_PAINT_TYPE_COLOR)
      glColor4fv((GLfloat*)&fill->color);
  else glColor4f(1,1,1,1);
  
  
  /* Check image drawing mode */
  if (context->imageMode == VG_DRAW_IMAGE_MULTIPLY &&
      fill->type != VG_PAINT_TYPE_COLOR) {
    
    /* Draw image quad into stencil */
    Driver::setBlendEnable(false);
    glDisable(GL_TEXTURE_2D);
	Driver::setDepthStencilState(stencilState_quadTo);
    Driver::setColorWriteMask(Driver::BlendState::DisableAll);

    Render::Driver::drawQuad(
      0, 0,
      (float)i->width, 0,
      (float)i->width, (float)i->height,
      0, (float)i->height,
      0.0f,
      (rhuint8)255u, (rhuint8)255u, (rhuint8)255u, (rhuint8)255u
    );

    /* Setup blending */
    updateBlendingStateGL(context, 0);
    
    /* Draw gradient mesh where stencil 1*/
    glEnable(GL_TEXTURE_2D);
	if (context->fillRule == VG_EVEN_ODD)
		Driver::setDepthStencilState(stencilState_drawOdd);
	else
		Driver::setDepthStencilState(stencilState_drawNonZero);
    
    SET2(min,0,0);
    SET2(max, (SHfloat)i->width, (SHfloat)i->height);
    if (fill->type == VG_PAINT_TYPE_RADIAL_GRADIENT) {
      shDrawRadialGradientMesh(fill, &min, &max, VG_FILL_PATH, GL_TEXTURE1);
    }else if (fill->type == VG_PAINT_TYPE_LINEAR_GRADIENT) {
      shDrawLinearGradientMesh(fill, &min, &max, VG_FILL_PATH, GL_TEXTURE1);
    }else if (fill->type == VG_PAINT_TYPE_PATTERN) {
      shDrawPatternMesh(fill, &min, &max, VG_FILL_PATH, GL_TEXTURE1); }
    
    glActiveTexture(GL_TEXTURE0);
    glDisable(GL_TEXTURE_2D);
    Driver::setStencilTestEnable(false);
    
  }else if (context->imageMode == VG_DRAW_IMAGE_STENCIL) {
    
    
  }else{/* Either normal mode or multiplying with a color-paint */
    
    /* Setup blending */
    updateBlendingStateGL(context, 0);

    /* Draw textured quad */
    glEnable(GL_TEXTURE_2D);
    
    glBegin(GL_QUADS);
    glVertex2i(0, 0);
    glVertex2i(i->width, 0);
    glVertex2i(i->width, i->height);
    glVertex2i(0, i->height);
    glEnd();
    
    glDisable(GL_TEXTURE_2D);
  }
  
  
  glDisable(GL_TEXTURE_GEN_S);
  glDisable(GL_TEXTURE_GEN_T);
  glPopMatrix();
  
  VG_RETURN(VG_NO_RETVAL);
}
