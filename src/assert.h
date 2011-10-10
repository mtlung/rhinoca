#ifndef __ASSERT_H__
#define __ASSERT_H__

void rhAssert(const wchar_t* expression, const wchar_t* file, unsigned line);

#if defined(_MSC_VER) && !defined(_DEBUG) && !defined(NDEBUG)
#	define NDEBUG
#endif

#ifdef NDEBUG
#	define RHASSERT(expression) ((void)0)
#	define RHVERIFY(expression) ((void)(expression))
#else
#	define __RHWIDEN(str) L ## str
#	define _RHWIDEN(str) __RHWIDEN(str)
#	define RHASSERT(expression) (void)( (!!(expression)) || (rhAssert(_RHWIDEN(#expression), _RHWIDEN(__FILE__), __LINE__), 0) )
#	define RHVERIFY(expression) RHASSERT(expression)
#endif

#endif	// __ASSERT_H__
