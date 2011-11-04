#ifndef __ASSERT_H__
#define __ASSERT_H__

// Declar rhAssert()
#ifdef _MSC_VER
void rhAssert(const wchar_t* expression, const wchar_t* file, unsigned line);
#else
#	include <assert.h>
#endif

#if defined(_MSC_VER) && !defined(_DEBUG) && !defined(NDEBUG)
#	define NDEBUG
#endif

// Declar RHASSERT and RHVERIFY
#ifdef NDEBUG
#	define RHASSERT(expression) ((void)0)
#	define RHVERIFY(expression) ((void)(expression))
#else
#	define __RHWIDEN(str) L ## str
#	define _RHWIDEN(str) __RHWIDEN(str)
#	ifdef _MSC_VER
#		define RHASSERT(expression) (void)( (!!(expression)) || (rhAssert(_RHWIDEN(#expression), _RHWIDEN(__FILE__), __LINE__), 0) )
#	else
#		define RHASSERT(expression) assert(expression)
#	endif
#	define RHVERIFY(expression) RHASSERT(expression)
#endif

// Declar RHSTATICASSERT
#define RHSTATICASSERT(expression) typedef char __static_assert_t[(expression)]

#endif	// __ASSERT_H__
