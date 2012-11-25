#ifndef __roUtility_h__
#define __roUtility_h__

#include "../platform/roCompiler.h"

void roMemcpy(void* dst, const void* src, roSize size);
void roMemmov(void* dst, const void* src, roSize size);

void roZeroMemory(void* dst, roSize size);
void roSwapMemory(void* p1, void* p2, roSize size);

/// The generic version of swap
template<typename T> inline
void roSwap(T& lhs, T& rhs) { T tmp(lhs); lhs = rhs; rhs = tmp; }

template<typename T> inline
const T& roMinOf2(const T& v1, const T& v2) { return v1 < v2 ? v1 : v2; }

template<typename T> inline
const T& roMaxOf2(const T& v1, const T& v2) { return v1 > v2 ? v1 : v2; }

template<typename T> inline
const T& roMinOf3(const T& v1, const T& v2, const T& v3) { return roMinOf2(v1, roMinOf2(v2, v3)); }

template<typename T> inline
const T& roMaxOf3(const T& v1, const T& v2, const T& v3) { return roMaxOf2(v1, roMaxOf2(v2, v3)); }

template<typename T> inline
const T& roMinOf4(const T& v1, const T& v2, const T& v3, const T& v4) { return roMinOf2(roMinOf2(v1, v2), roMinOf2(v3, v4)); }

template<typename T> inline
const T& roMaxOf4(const T& v1, const T& v2, const T& v3, const T& v4) { return roMaxOf2(roMaxOf2(v1, v2), roMaxOf2(v3, v4)); }

template<typename T> inline
T roAlignFloor(const T& val, const T& alignTo) { return val - (val % alignTo); }

template<typename T> inline
T roAlignCeiling(const T& val, const T& alignTo) { T d = val % alignTo; return d == 0 ? val : val - d + alignTo; }

#endif	// __roUtility_h__
