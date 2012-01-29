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
#include "shPaint.h"
#include <stdio.h>

#define _ITEM_T SHStop
#define _ARRAY_T SHStopArray
#define _FUNC_T shStopArray
#define _COMPARE_T(s1,s2) 0
#define _ARRAY_DEFINE
#include "shArrayBase.h"

#define _ITEM_T SHPaint*
#define _ARRAY_T SHPaintArray
#define _FUNC_T shPaintArray
#define _ARRAY_DEFINE
#include "shArrayBase.h"

#include "../roRenderDriver.h"
#include "../../base/roStringHash.h"

#include "shImage.h"
#define _ITEM_T SHImage*
#define _ARRAY_T SHImageArray
#define _FUNC_T shImageArray
#define _ARRAY_DEFINE
#include "shArrayBase.h"
void SHImage_dtor(SHImage *i)
{
}

//using namespace ro;

void SHPaint_ctor(SHPaint *p)
{
	VG_GETCONTEXT(VG_NO_RETVAL);
	int i;

	p->type = VG_PAINT_TYPE_COLOR;
	CSET(p->color, 0,0,0,1);
	SH_INITOBJ(SHStopArray, p->instops);
	SH_INITOBJ(SHStopArray, p->stops);
	p->premultiplied = VG_FALSE;
	p->spreadMode = VG_COLOR_RAMP_SPREAD_PAD;
	p->tilingMode = VG_TILE_FILL;
	for (i=0; i<4; ++i) p->linearGradient[i] = 0.0f;
	for (i=0; i<5; ++i) p->radialGradient[i] = 0.0f;
	p->texture = NULL;
	p->pattern = VG_INVALID_HANDLE;
}

void SHPaint_dtor(SHPaint *p)
{
	VG_GETCONTEXT(VG_NO_RETVAL);

	SH_DEINITOBJ(SHStopArray, p->instops);
	SH_DEINITOBJ(SHStopArray, p->stops);

	context->driver->deleteTexture(p->texture);
}

VG_API_CALL VGPaint vgCreatePaint(void)
{
	SHPaint *p = NULL;
	VG_GETCONTEXT(VG_INVALID_HANDLE);

	// Create new paint object 
	SH_NEWOBJ(SHPaint, p);
	VG_RETURN_ERR_IF(!p, VG_OUT_OF_MEMORY_ERROR,
		VG_INVALID_HANDLE);

	// Add to resource list 
	shPaintArrayPushBack(&context->paints, p);

	p->texture = context->driver->newTexture();
	context->driver->initTexture(p->texture, SH_GRADIENT_TEX_SIZE, 1, roRDriverTextureFormat_RGBA, roRDriverTextureFlag_None);

	VG_RETURN((VGPaint)p);
}

VG_API_CALL void vgDestroyPaint(VGPaint paint)
{
	SHint index;
	VG_GETCONTEXT(VG_NO_RETVAL);

	// Check if handle valid 
	index = shPaintArrayFind(&context->paints, (SHPaint*)paint);
	VG_RETURN_ERR_IF(index == -1, VG_BAD_HANDLE_ERROR, VG_NO_RETVAL);

	// Delete object and remove resource 
	SH_DELETEOBJ(SHPaint, (SHPaint*)paint);
	shPaintArrayRemoveAt(&context->paints, index);

	VG_RETURN(VG_NO_RETVAL);
}

VG_API_CALL void vgSetPaint(VGPaint paint, VGbitfield paintModes)
{
	VG_GETCONTEXT(VG_NO_RETVAL);

	// Check if handle valid 
	VG_RETURN_ERR_IF(!shIsValidPaint(context, paint) &&
		paint != VG_INVALID_HANDLE,
		VG_BAD_HANDLE_ERROR, VG_NO_RETVAL);

	// Check for invalid mode 
	VG_RETURN_ERR_IF(paintModes & ~(VG_STROKE_PATH | VG_FILL_PATH),
		VG_ILLEGAL_ARGUMENT_ERROR, VG_NO_RETVAL);

	// Set stroke / fill 
	if (paintModes & VG_STROKE_PATH)
		context->strokePaint = (SHPaint*)paint;
	if (paintModes & VG_FILL_PATH)
		context->fillPaint = (SHPaint*)paint;

	VG_RETURN(VG_NO_RETVAL);
}

VG_API_CALL void vgPaintPattern(VGPaint paint, VGImage pattern)
{
	VG_GETCONTEXT(VG_NO_RETVAL);

	// Check if handle valid 
	VG_RETURN_ERR_IF(!shIsValidPaint(context, paint),
		VG_BAD_HANDLE_ERROR, VG_NO_RETVAL);

	// Check if pattern image valid 
	VG_RETURN_ERR_IF(!shIsValidImage(context, pattern),
		VG_BAD_HANDLE_ERROR, VG_NO_RETVAL);

	// TODO: Check if pattern image is current rendering target 

	// Set pattern image 
	((SHPaint*)paint)->pattern = pattern;

	VG_RETURN(VG_NO_RETVAL);
}

void shUpdateColorRampTexture(SHPaint *p)
{
	VG_GETCONTEXT(VG_NO_RETVAL);

	SHint s=0;
	SHStop *stop1, *stop2;
	SHuint8 rgba[SH_GRADIENT_TEX_COORDSIZE];
	SHint x1=0, x2=0, dx, x;
	SHColor dc, c;
	SHfloat k;

	// Write first pixel color 
	stop1 = &p->stops.items[0];
	CSTORE_RGBA1D_8(stop1->color, rgba, x1);

	// Walk stops 
	for (s=1; s<p->stops.size; ++s, x1=x2, stop1=stop2)
	{
		// Pick next stop 
		stop2 = &p->stops.items[s];
		x2 = (SHint)(stop2->offset * (SH_GRADIENT_TEX_SIZE-1));

		SH_ASSERT(
			x1 >= 0 && x1 < SH_GRADIENT_TEX_SIZE &&
			x2 >= 0 && x2 < SH_GRADIENT_TEX_SIZE &&
			x1 <= x2);

		dx = x2 - x1;
		CSUBCTO(stop2->color, stop1->color, dc);

		// Interpolate in between 
		for (x=x1+1; x<=x2; ++x) {
			k = (SHfloat)(x-x1)/dx;
			CSETC(c, stop1->color);
			CADDCK(c, dc, k);
			CSTORE_RGBA1D_8(c, rgba, x);
		}
	}

	// Update texture image 
	context->driver->commitTexture(p->texture, rgba, 0);
}

void shValidateInputStops(SHPaint *p)
{
	SHStop *instop, stop;
	SHfloat lastOffset=0.0f;
	int i;

	shStopArrayClear(&p->stops);
	shStopArrayReserve(&p->stops, p->instops.size);

	// Assure input stops are properly defined 
	for (i=0; i<p->instops.size; ++i)
	{
		// Copy stop color 
		instop = &p->instops.items[i];
		stop.color = instop->color;

		// Offset must be in [0,1] 
		if (instop->offset < 0.0f || instop->offset > 1.0f)
			continue;

		// Discard whole sequence if not in ascending order 
		if (instop->offset < lastOffset)
		{shStopArrayClear(&p->stops); break;}

		// Add stop at offset 0 with same color if first not at 0 
		if (p->stops.size == 0 && instop->offset != 0.0f) {
			stop.offset = 0.0f;
			shStopArrayPushBackP(&p->stops, &stop);}

		// Add current stop to array 
		stop.offset = instop->offset;
		shStopArrayPushBackP(&p->stops, &stop);

		// Save last offset 
		lastOffset = instop->offset;
	}

	// Add stop at offset 1 with same color if last not at 1 
	if (p->stops.size > 0 && lastOffset != 1.0f) {
		stop.offset = 1.0f;
		shStopArrayPushBackP(&p->stops, &stop);
	}

	// Add 2 default stops if no valid found 
	if (p->stops.size == 0) {
		// First opaque black 
		stop.offset = 0.0f;
		CSET(stop.color, 0,0,0,1);
		shStopArrayPushBackP(&p->stops, &stop);
		// Last opaque white 
		stop.offset = 1.0f;
		CSET(stop.color, 1,1,1,1);
		shStopArrayPushBackP(&p->stops, &stop);
	}

	// Update texture 
	shUpdateColorRampTexture(p);
}

void shGenerateStops(SHPaint *p, SHfloat minOffset, SHfloat maxOffset,
                     SHStopArray *outStops)
{
	SHStop *s1,*s2;
	SHint i1,i2;
	SHfloat o=0.0f;
	SHfloat ostep=0.0f;
	SHint istep=1;
	SHint istart=0;
	SHint iend=p->stops.size-1;
	SHint minDone=0;
	SHint maxDone=0;
	SHStop outStop;

	// Start below zero? 
	if (minOffset < 0.0f)
	{
		if (p->spreadMode == VG_COLOR_RAMP_SPREAD_PAD) {
			// Add min offset stop 
			outStop = p->stops.items[0];
			outStop.offset = minOffset;
			shStopArrayPushBackP(outStops, &outStop);
			// Add max offset stop and exit 
			if (maxOffset < 0.0f) {
				outStop.offset = maxOffset;
				shStopArrayPushBackP(outStops, &outStop);
				return;
			}
		}
		else {
			// Pad starting offset to nearest factor of 2 
			SHint ioff = (SHint)SH_FLOOR(minOffset);
			o = (SHfloat)(ioff - (ioff & 1));
		}
	}

	// Construct stops until max offset reached 
	for (i1=istart, i2=istart+istep; maxDone!=1;
		i1+=istep, i2+=istep, o+=ostep)
	{
		// All stops consumed? 
		if (i1==iend) { switch(p->spreadMode)
		{
		case VG_COLOR_RAMP_SPREAD_PAD:
		  // Pick last stop 
		  outStop = p->stops.items[i1];
		  if (!minDone) {
			  // Add min offset stop with last color 
			  outStop.offset = minOffset;
			  shStopArrayPushBackP(outStops, &outStop); }
		  // Add max offset stop with last color 
		  outStop.offset = maxOffset;
		  shStopArrayPushBackP(outStops, &outStop);
		  return;

		case VG_COLOR_RAMP_SPREAD_REPEAT:
		  // Reset iteration 
		  i1=istart; i2=istart+istep;
		  // Add stop1 if past min offset 
		  if (minDone) {
			  outStop = p->stops.items[0];
			  outStop.offset = o;
			  shStopArrayPushBackP(outStops, &outStop); }
		  break;

		case VG_COLOR_RAMP_SPREAD_REFLECT:
			// Reflect iteration direction 
			istep = -istep;
			i2 = i1 + istep;
			iend = (istep==1) ? p->stops.size-1 : 0;
			break;
		}}

		// 2 stops and their offset distance 
		s1 = &p->stops.items[i1];
		s2 = &p->stops.items[i2];
		ostep = s2->offset - s1->offset;
		ostep = SH_ABS(ostep);

		// Add stop1 if reached min offset 
		if (!minDone && o+ostep > minOffset) {
			minDone = 1;
			outStop = *s1;
			outStop.offset = o;
			shStopArrayPushBackP(outStops, &outStop);
		}

		// Mark done if reached max offset 
		if (o+ostep > maxOffset)
			maxDone = 1;

		// Add stop2 if past min offset 
		if (minDone) {
			outStop = *s2;
			outStop.offset = o+ostep;
			shStopArrayPushBackP(outStops, &outStop);
		}
	}
}

static float whiteColor[4] = { 1, 1, 1, 1 };

static const ro::StringHash texUniform[2] = { ro::stringHash("texGrad"), ro::stringHash("texImage") };

int shSetGradientTexGLState(SHPaint *p, unsigned texUnit)
{
	SH_GETCONTEXT(0);

	roRDriverTextureAddressMode mode;
	switch (p->spreadMode) {
	case VG_COLOR_RAMP_SPREAD_PAD:
		mode = roRDriverTextureAddressMode_Edge; break;
	case VG_COLOR_RAMP_SPREAD_REPEAT:
		mode = roRDriverTextureAddressMode_Repeat; break;
	case VG_COLOR_RAMP_SPREAD_REFLECT:
		mode = roRDriverTextureAddressMode_MirrorRepeat; break;
	default:
		mode = roRDriverTextureAddressMode_Edge; break;
	}

	roRDriverTextureState state = {
		0,
		roRDriverTextureFilterMode_MinMagLinear,
		mode,
		mode,
		1
	};

	context->driver->setTextureState(&state, 1, texUnit);
	context->driver->setUniformTexture(texUniform[texUnit], p->texture);
	context->driver->updateBuffer(context->uBuffer, roOffsetof(UniformBuffer, UniformBuffer::color), whiteColor, sizeof(SHColor));

	return 1;
}

void shSetPatternTexGLState(SHPaint *p, VGContext *c, unsigned texUnit)
{
	roRDriverTextureAddressMode mode;
	switch (p->tilingMode) {
	case VG_TILE_FILL:
		mode = roRDriverTextureAddressMode_Border; break;
	case VG_TILE_PAD:
		mode = roRDriverTextureAddressMode_Edge; break;
	case VG_TILE_REPEAT:
		mode = roRDriverTextureAddressMode_Repeat; break;
	case VG_TILE_REFLECT:
		mode = roRDriverTextureAddressMode_MirrorRepeat; break;
	default:
		mode = roRDriverTextureAddressMode_Edge; break;
	}

	roRDriverTextureState state =  {
		0,
		roRDriverTextureFilterMode_MinMagLinear,
		mode,
		mode,
		1
	};

	c->driver->setTextureState(&state, 1, 0);
	c->driver->setUniformTexture(texUniform[texUnit], ((SHImage*)p->pattern)->texture);
	c->driver->updateBuffer(c->uBuffer, roOffsetof(UniformBuffer, UniformBuffer::color), whiteColor, sizeof(SHColor));
}

static void fillBoundingBox(VGContext* c, SHVector2* corners, SHColor* color)
{
	float vertex[6*2] = {
		corners[0].x,corners[0].y, corners[1].x,corners[1].y, corners[2].x,corners[2].y,
		corners[2].x,corners[2].y, corners[3].x,corners[3].y, corners[0].x,corners[0].y,
	};

	roVerify(c->driver->updateBuffer(c->quadBuffer, 0, vertex, sizeof(vertex)));
	roVerify(c->driver->bindShaderInput(c->quadInputLayout, roCountof(c->quadInputLayout), NULL));

	c->driver->updateBuffer(c->uBuffer, roOffsetof(UniformBuffer, UniformBuffer::color), color, sizeof(SHColor));
	c->driver->drawTriangle(0, 6, 0);
}

int shDrawLinearGradientMesh(SHPaint *p, SHVector2 *min, SHVector2 *max,
                             VGPaintMode mode, unsigned texUnit)
{
	SHint i;
	SHfloat n;

	SHfloat x1 = p->linearGradient[0];
	SHfloat y1 = p->linearGradient[1];
	SHfloat x2 = p->linearGradient[2];
	SHfloat y2 = p->linearGradient[3];
	SHVector2 c, ux, uy;
	SHVector2 cc, uux, uuy;

	SHMatrix3x3 *m;
	SHMatrix3x3 mi;
	SHint invertible;
	SHVector2 corners[4];
	SHfloat minOffset = 0.0f;
	SHfloat maxOffset = 0.0f;
	SHfloat left = 0.0f;
	SHfloat right = 0.0f;
	SHVector2 l1,r1,l2,r2;

	// Pick paint transform matrix 
	SH_GETCONTEXT(0);
	if (mode == VG_FILL_PATH)
		m = &context->fillTransform;
	else if (mode == VG_STROKE_PATH)
		m = &context->strokeTransform;

	// Gradient center and unit vectors 
	SET2(c, x1, y1);
	SET2(ux, x2-x1, y2-y1);
	SET2(uy, -ux.y, ux.x);
	n = NORM2(ux);
	DIV2(ux, n);
	NORMALIZE2(uy);

	// Apply paint-to-user transformation 
	ADD2V(ux, c); ADD2V(uy, c);
	TRANSFORM2TO(c, (*m), cc);
	TRANSFORM2TO(ux, (*m), uux);
	TRANSFORM2TO(uy, (*m), uuy);
	SUB2V(ux,c); SUB2V(uy,c);
	SUB2V(uux,cc); SUB2V(uuy,cc);

	// Boundbox corners 
	SET2(corners[0], min->x, min->y);
	SET2(corners[1], max->x, min->y);
	SET2(corners[2], max->x, max->y);
	SET2(corners[3], min->x, max->y);

	// Find inverse transformation (back to paint space) 
	invertible = shInvertMatrix(m, &mi);
	if (!invertible || n==0.0f) {
		// Fill boundbox with color at offset 1 
		SHColor *c = &p->stops.items[p->stops.size-1].color;
		fillBoundingBox(context, corners, c);
		return 1;
	}

	//--------------------------------------------------------

	for (i=0; i<4; ++i) {
		// Find min/max offset and perpendicular span 
		SHfloat o, s;
		TRANSFORM2(corners[i], mi);
		SUB2V(corners[i], c);
		o = DOT2(corners[i], ux) / n;
		s = DOT2(corners[i], uy);
		if (o < minOffset || i==0) minOffset = o;
		if (o > maxOffset || i==0) maxOffset = o;
		if (s < left || i==0) left = s;
		if (s > right || i==0) right = s;
	}

	//---------------------------------------------------------

	// Corners of boundbox in gradient system 
	SET2V(l1, cc); SET2V(r1, cc);
	SET2V(l2, cc); SET2V(r2, cc);
	OFFSET2V(l1, uuy, left);  OFFSET2V(l1, uux, minOffset * n);
	OFFSET2V(r1, uuy, right); OFFSET2V(r1, uux, minOffset * n);
	OFFSET2V(l2, uuy, left);  OFFSET2V(l2, uux, maxOffset * n);
	OFFSET2V(r2, uuy, right); OFFSET2V(r2, uux, maxOffset * n);

	// Draw quad using color-ramp texture
	shSetGradientTexGLState(p, texUnit);

	float vertex[4*2*2] = {
		r1.x,r1.y,				l1.x,l1.y,				r2.x,r2.y,				l2.x,l2.y,
		minOffset,minOffset,	minOffset,minOffset,	maxOffset,maxOffset,	maxOffset,maxOffset,
	};

	roVerify(context->driver->updateBuffer(context->quadBuffer, 0, vertex, sizeof(vertex)));
	roVerify(context->driver->bindShaderInput(context->quadInputLayout, roCountof(context->quadInputLayout), NULL));
	context->driver->drawPrimitive(roRDriverPrimitiveType_TriangleStrip, 0, 4, 0);

	return 1;
}

int shDrawRadialGradientMesh(SHPaint *p, SHVector2 *min, SHVector2 *max,
                             VGPaintMode mode, unsigned texUnit)
{
	SHint i, j;
	float a, n;

	SHfloat cx = p->radialGradient[0];
	SHfloat cy = p->radialGradient[1];
	SHfloat fx = p->radialGradient[2];
	SHfloat fy = p->radialGradient[3];
	float r = p->radialGradient[4];
	float fcx, fcy, rr, C;

	SHVector2 ux;
	SHVector2 uy;
	SHVector2 c, f;
	SHVector2 cf;

	SHMatrix3x3 *m;
	SHMatrix3x3 mi;
	SHint invertible;
	SHVector2 corners[4];
	SHVector2 fcorners[4];
	SHfloat minOffset=0.0f;
	SHfloat maxOffset=0.0f;

	SHint maxI=0, maxJ=0;
	SHfloat maxA=0.0f;
	SHfloat startA=0.0f;

	int numsteps = 100;
	float step = 2*PI/numsteps;
	SHVector2 tmin, tmax;
	SHVector2 min1, max1, min2, max2;

	// Pick paint transform matrix 
	SH_GETCONTEXT(0);
	if (mode == VG_FILL_PATH)
		m = &context->fillTransform;
	else if (mode == VG_STROKE_PATH)
		m = &context->strokeTransform;

	// Move focus into circle if outside 
	SET2(cf, fx,fy);
	SUB2(cf, cx,cy);
	n = NORM2(cf);
	if (n > r) {
		DIV2(cf, n);
		fx = cx + 0.995f * r * cf.x;
		fy = cy + 0.995f * r * cf.y;
	}

	// Precalculations 
	rr = r*r;
	fcx = fx - cx;
	fcy = fy - cy;
	C = fcx*fcx + fcy*fcy - rr;

	// Apply paint-to-user transformation
	// to focus and unit vectors
	SET2(f, fx, fy);
	SET2(c, cx, cy);
	SET2(ux, 1, 0);
	SET2(uy, 0, 1);
	ADD2(ux, cx, cy);
	ADD2(uy, cx, cy);
	TRANSFORM2(f, (*m));
	TRANSFORM2(c, (*m));
	TRANSFORM2(ux, (*m));
	TRANSFORM2(uy, (*m));
	SUB2V(ux, c); SUB2V(uy, c);

	// Boundbox corners 
	SET2(corners[0], min->x, min->y);
	SET2(corners[1], max->x, min->y);
	SET2(corners[2], max->x, max->y);
	SET2(corners[3], min->x, max->y);

	// Find inverse transformation (back to paint space) 
	invertible = shInvertMatrix(m, &mi);
	if (!invertible || r <= 0.0f) {
		// Fill boundbox with color at offset 1 
		SHColor *c = &p->stops.items[p->stops.size-1].color;
		fillBoundingBox(context, corners, c);
		return 1;
	}

	/*--------------------------------------------------------*/

	// Find min/max offset 
	for (i=0; i<4; ++i)
	{
		// Transform to paint space 
		SHfloat ax,ay, A,B,D,t, off;
		TRANSFORM2TO(corners[i], mi, fcorners[i]);
		SUB2(fcorners[i], fx, fy);
		n = NORM2(fcorners[i]);
		if (n == 0.0f) {
			// Avoid zero-length vectors 
			off = 0.0f;
		}
		else {
			// Distance from focus to circle at corner angle 
			DIV2(fcorners[i], n);
			ax = fcorners[i].x;
			ay = fcorners[i].y;
			A = ax*ax + ay*ay;
			B = 2 * (fcx*ax + fcy*ay);
			D = B*B - 4*A*C;
			t = (-B + SH_SQRT(D)) / (2*A);

			// Relative offset of boundbox corner 
			if (D <= 0.0f) off = 1.0f;
			else off = n / t;
		}

		// Find smallest and largest offset 
		if (off < minOffset || i==0) minOffset = off;
		if (off > maxOffset || i==0) maxOffset = off;
	}

	// Is transformed focus inside original boundbox? 
	if (f.x >= min->x && f.x <= max->x &&
		f.y >= min->y && f.y <= max->y)
	{
			// Draw whole circle 
			minOffset = 0.0f;
			startA = 0.0f;
			maxA = 2*PI;
	}
	else
	{
		// Find most distant corner pair 
		for (i=0; i<3; ++i) {
			if (ISZERO2(fcorners[i])) continue;
			for (j=i+1; j<4; ++j) {
				if (ISZERO2(fcorners[j])) continue;
				a = ANGLE2N(fcorners[i], fcorners[j]);
				if (a > maxA || maxA == 0.0f)
				{maxA=a; maxI=i; maxJ=j;}
			}
		}

		// Pick starting angle 
		if (CROSS2(fcorners[maxI],fcorners[maxJ]) > 0.0f)
			startA = shVectorOrientation(&fcorners[maxI]);
		else startA = shVectorOrientation(&fcorners[maxJ]);
	}

	/*---------------------------------------------------------*/

	// TODO: for minOffset we'd actually need to find minimum
	// of the gradient function when X and Y are substituted
	// with a line equation for each bound-box edge. As a
	// workaround we use 0.0f for now.
	minOffset = 0.0f;
	step = PI/50;
	numsteps = (SHint)SH_CEIL(maxA / step) + 1;

	shSetGradientTexGLState(p, texUnit);

	const unsigned vertexSize = 6 * 2 * 2;
	roRDriverBuffer* vBuf = context->driver->newBuffer();
	roVerify(context->driver->initBuffer(vBuf, roRDriverBufferType_Vertex, roRDriverDataUsage_Stream, NULL, (numsteps-1)*sizeof(float)*vertexSize));
	context->inputLayout[0].buffer = vBuf;
	roVerify(context->driver->bindShaderInput(context->inputLayout, roCountof(context->inputLayout), NULL));

	// Walk the steps and draw gradient mesh 
	for (i=0, a=startA; i<numsteps; ++i, a+=step)
	{
		// Distance from focus to circle border
		// at current angle (gradient space)
		float ax, ay;
		roSinCos(a, ay, ax);
		float A = ax*ax + ay*ay;
		float B = 2 * (fcx*ax + fcy*ay);
		float D = B*B - 4*A*C;
		float t = (-B + SH_SQRT(D)) / (2*A);
		if (D <= 0.0f) t = 0.0f;

		// Vectors pointing towards minimum and maximum
		// offset at current angle (gradient space)
		tmin.x = ax * t * minOffset;
		tmin.y = ay * t * minOffset;
		tmax.x = ax * t * maxOffset;
		tmax.y = ay * t * maxOffset;

		// Transform back to user space 
		min2.x = f.x + tmin.x * ux.x + tmin.y * uy.x;
		min2.y = f.y + tmin.x * ux.y + tmin.y * uy.y;
		max2.x = f.x + tmax.x * ux.x + tmax.y * uy.x;
		max2.y = f.y + tmax.x * ux.y + tmax.y * uy.y;

		// Draw quad
		if (i!=0) {
			float vertex[vertexSize] = {
				min1.x,min1.y,			max1.x,max1.y,			min2.x,min2.y,
				max1.x,max1.y,			max2.x,max2.y,			min2.x,min2.y,
				minOffset,minOffset,	minOffset,minOffset,	maxOffset,maxOffset,
				minOffset,minOffset,	maxOffset,maxOffset,	maxOffset,maxOffset,
			};
			// TODO: Use mapBuffer may be faster
			context->driver->updateBuffer(vBuf, (i-1) * sizeof(vertex), vertex, sizeof(vertex));
		}

		// Save prev points
		min1 = min2;
		max1 = max2;
	}

	context->driver->drawTriangle(0, (numsteps-1)*6, 0);
	context->driver->deleteBuffer(vBuf);

	return 1;
}

int shDrawPatternMesh(SHPaint *p, SHVector2 *min, SHVector2 *max,
                      VGPaintMode mode, unsigned texUnit)
{
	SHMatrix3x3 *m;
	SHMatrix3x3 mi;
	SHfloat migl[16];
	SHint invertible;
	SHVector2 corners[4];
	VGfloat sx, sy;
	SHImage *img;

	// Pick paint transform matrix 
	SH_GETCONTEXT(0);
	if (mode == VG_FILL_PATH)
		m = &context->fillTransform;
	else if (mode == VG_STROKE_PATH)
		m = &context->strokeTransform;

	// Boundbox corners 
	SET2(corners[0], min->x, min->y);
	SET2(corners[1], max->x, min->y);
	SET2(corners[2], max->x, max->y);
	SET2(corners[3], min->x, max->y);

	// Find inverse transformation (back to paint space) 
	invertible = shInvertMatrix(m, &mi);
	if (!invertible) {
		// Fill boundbox with tile fill color
		SHColor *c = &context->tileFillColor;
		fillBoundingBox(context, corners, c);
		return 1;
	}

	// Setup texture coordinate transform 
	img = (SHImage*)p->pattern;
	sx = 1.0f/(VGfloat)img->texwidth;
	sy = 1.0f/(VGfloat)img->texheight;

	shMatrixToGL(&mi, migl);
//	glMatrixMode(GL_TEXTURE);
//	glPushMatrix();
//	glScalef(sx, sy, 1.0f);
//	glMultMatrixf(migl);

	// Draw boundbox with same texture coordinates
	// that will get transformed back to paint space
	shSetPatternTexGLState(p, context, texUnit);

	roVerify(context->driver->updateBuffer(context->quadBuffer, 0, corners, sizeof(corners)));
	roVerify(context->driver->bindShaderInput(context->quadInputLayout, roCountof(context->quadInputLayout), NULL));
	context->driver->drawPrimitive(roRDriverPrimitiveType_TriangleStrip, 0, 4, 0);

//	for (int i=0; i<4; ++i) {
//		glMultiTexCoord2f(texUnit, corners[i].x, corners[i].y);
//		glVertex2fv((GLfloat*)&corners[i]);
//	}

	return 1;
}
