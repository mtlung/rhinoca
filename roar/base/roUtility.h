#ifndef __roUtility_h__
#define __roUtility_h__

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

#endif	// __roUtility_h__
