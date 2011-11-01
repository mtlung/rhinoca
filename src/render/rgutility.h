#ifndef __RENDER_RGUTILITY_H__
#define __RENDER_RGUTILITY_H__

#ifdef __cplusplus
extern "C" {
#endif

void rgMat44MakeIdentity(float outMat[16]);

void rgMat44MakePrespective(float outMat[16], float fovyDeg, float acpectWidthOverHeight, float znera, float zfar);

void rgMat44MulVec4(const float mat[16], const float in[4], float out[4]);

void rgMat44Transform(const float mat[16], float xyz[3]);

void rgMat44TranslateBy(float outMat[16], const float xyz[3]);

void rgMat44Transpose(float mat[16]);

#ifdef __cplusplus
}
#endif

#endif	// __RENDER_RGUTILITY_H__
