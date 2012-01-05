#include "pch.h"
#include "roMatrix.h"
#include "../platform/roCompiler.h"
#include "../platform/roOS.h"

#if roCPU_SSE
#	include <xmmintrin.h>
#endif

namespace ro {

void mat4MulVec4(const float m[4][4], const float src[4], float dst[4])
{
#if roCPU_SSE
	__m128 x0 = _mm_loadu_ps(m[0]);
	__m128 x1 = _mm_loadu_ps(m[1]);
	__m128 x2 = _mm_loadu_ps(m[2]);
	__m128 x3 = _mm_loadu_ps(m[3]);

	__m128 v = _mm_loadu_ps(src);
	__m128 v0 = _mm_shuffle_ps(v, v, _MM_SHUFFLE(0,0,0,0));
	__m128 v1 = _mm_shuffle_ps(v, v, _MM_SHUFFLE(1,1,1,1));
	__m128 v2 = _mm_shuffle_ps(v, v, _MM_SHUFFLE(2,2,2,2));
	__m128 v3 = _mm_shuffle_ps(v, v, _MM_SHUFFLE(3,3,3,3));

	v0 = _mm_mul_ps(x0, v0);
	v1 = _mm_mul_ps(x1, v1);
	v2 = _mm_mul_ps(x2, v2);
	v3 = _mm_mul_ps(x3, v3);

	x0 = _mm_add_ps(v0, v1);
	x1 = _mm_add_ps(v2, v3);
	v = _mm_add_ps(x0, x1);

	_mm_storeu_ps(dst, v);
#else
	// Local variables to prevent parameter aliasing
	const float x = src[0];
	const float y = src[1];
	const float z = src[2];
	const float w = src[3];

	dst[0] = m[0][0] * x + m[0][1] * y + m[0][2] * z + m[0][3] * w;
	dst[1] = m[1][0] * x + m[1][1] * y + m[1][2] * z + m[1][3] * w;
	dst[2] = m[2][0] * x + m[2][1] * y + m[2][2] * z + m[2][3] * w;
	dst[3] = m[3][0] * x + m[3][1] * y + m[3][2] * z + m[3][3] * w;
#endif
}

void mat4MulMat4(const float lhs[4][4], const float rhs[4][4], float dst[4][4])
{
	roAssert(lhs != rhs);
	roAssert(lhs != dst);

#if roCPU_SSE
	// Reference for SSE matrix multiplication:
	// http://www.cortstratton.org/articles/OptimizingForSSE.php
	__m128 x4 = _mm_loadu_ps(lhs[0]);
	__m128 x5 = _mm_loadu_ps(lhs[1]);
	__m128 x6 = _mm_loadu_ps(lhs[2]);
	__m128 x7 = _mm_loadu_ps(lhs[3]);

	__m128 x0, x1, x2, x3;

	for(unsigned i=0; i<4; ++i) {
		x1 = x2 = x3 = x0 = _mm_loadu_ps(rhs[i]);
		x0 = _mm_shuffle_ps(x0, x0, _MM_SHUFFLE(0,0,0,0));
		x1 = _mm_shuffle_ps(x1, x1, _MM_SHUFFLE(1,1,1,1));
		x2 = _mm_shuffle_ps(x2, x2, _MM_SHUFFLE(2,2,2,2));
		x3 = _mm_shuffle_ps(x3, x3, _MM_SHUFFLE(3,3,3,3));

		x0 = _mm_mul_ps(x0, x4);
		x1 = _mm_mul_ps(x1, x5);
		x2 = _mm_mul_ps(x2, x6);
		x3 = _mm_mul_ps(x3, x7);

		x2 = _mm_add_ps(x2, x0);
		x3 = _mm_add_ps(x3, x1);
		x3 = _mm_add_ps(x3, x2);

		_mm_storeu_ps(dst[i], x3);
	}
#else
#endif
}

}	// namespace ro
