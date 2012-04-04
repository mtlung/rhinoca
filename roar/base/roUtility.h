#ifndef __roUtility_h__
#define __roUtility_h__

#include "../platform/roCompiler.h"

void roMemcpy(void* dst, const void* src, roSize size);
void roMemmov(void* dst, const void* src, roSize size);

void roZeroMemory(void* dst, roSize size);

/// The generic version of swap
template<typename T> inline
void roSwap(T& lhs, T& rhs) { T tmp(lhs); lhs = rhs; rhs = tmp; }

/// To notify an object that it's being moved to a new memory location
template<typename T> inline
void roOnMemMove(T& obj, void* newMemLocation) {}

template<typename T> inline
const T& roMinOf2(const T& v1, const T& v2) { return v1 < v2 ? v1 : v2; }

template<typename T> inline
const T& roMaxOf2(const T& v1, const T& v2) { return v1 > v2 ? v1 : v2; }

template<typename T> inline
const T& roMinOf3(const T& v1, const T& v2, const T& v3) { return roMinOf2(v1, roMinOf2(v2, v3)); }

template<typename T> inline
const T& roMaxOf3(const T& v1, const T& v2, const T& v3) { return roMaxOf2(v1, roMaxOf2(v2, v3)); }

#endif	// __roUtility_h__
