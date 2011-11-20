#ifndef __roUtility_h__
#define __roUtility_h__

template<typename T>
void roSwap(T& lhs, T& rhs) { T tmp(lhs); lhs = rhs; rhs = tmp; }

template<typename T>
const T& roMinOf2(const T& v1, const T& v2) { return v1 < v2 ? v1 : v2; }

template<typename T>
const T& roMaxOf2(const T& v1, const T& v2) { return v1 > v2 ? v1 : v2; }

#endif	// __roUtility_h__
