#ifndef __COMMON_H__
#define __COMMON_H__

#include "rhinoca.h"
#include "intrusiveptr.h"
#include <assert.h>
#include <stdlib.h>	// For _countof

#ifdef NDEBUG
#	define ASSERT(Expression) ((void)0)
#	define VERIFY(Expression) ((void)(Expression))
#else
#	define ASSERT(Expression) assert(Expression)
#	define VERIFY(Expression) assert(Expression)
#endif

// Memory allocation
/// Typed version of malloc, use count of object rather than size in byte.
/// No constructor invoked!
template<typename T>
inline T* rhnew(rhuint count) {
	return reinterpret_cast<T*>(rhinoca_malloc(count * sizeof(T)));
}

/// No destructor invoked!
template<typename T>
inline void rhdelete(T* ptr) {
	rhinoca_free(ptr);
}

template<typename T>
inline T* rhrenew(const T* ptr, rhuint oldCount, rhuint newCount) {
	return reinterpret_cast<T*>(rhinoca_realloc((void*)ptr, oldCount * sizeof(T), newCount * sizeof(T)));
}

// IO
extern rhinoca_io_open io_open;
extern rhinoca_io_ready io_ready;
extern rhinoca_io_read io_read;
extern rhinoca_io_seek io_seek;
extern rhinoca_io_close io_close;
extern rhinoca_printFunc print;

double atof(const char* str, double onErr);
void tolower(char* str);

#if defined(_MSC_VER)
#define FORCE_INLINE __forceinline
int strcasecmp(const char* s1, const char* s2);
#else
#define FORCE_INLINE inline
#endif

#define override virtual

/*!	Macro to get the count of element of an array
	For the Visual Studio, in use the provided _countof macro defined in
	stdlib.h, which can prevent many miss use of it.
	\sa http://blogs.msdn.com/the1/archive/2004/05/07/128242.aspx
 */
#if defined(_MSC_VER)
#	define COUNTOF(x) _countof(x)
#else
#	define COUNTOF(x) (sizeof(x) / sizeof(typeof(x[0])))
#endif

/*! To get back the host object from a member variable.
	Use this macro to declare the \em GetOuter and \em GetOuterSafe functions
	to access the outer object.

	For example, you want to access the ClientInfo from it's member variable mActClient and mActServer:
	\code
	struct ClientInfo {
		struct ActivityClient : public LinkListBase::NodeBase {
			DECLAR_GET_OUTER_OBJ(ClientInfo, mActClient);
		} mActClient;

		struct ActivityServer : public LinkListBase::NodeBase {
			DECLAR_GET_OUTER_OBJ(ClientInfo, mActServer);
		} mActServer;
	};

	ClientInfo info;
	ClientInfo& infoRef = info.mActServer.getOuter();
	ClientInfo* infoPtr = info.mActServer.getOuterSafe();
	\endcode

	\note The same restriction of \em offsetof macro apply on \em DECLAR_GET_OUTER_OBJ
 */
#define DECLAR_GET_OUTER_OBJ(OuterClass, ThisVar) \
	OuterClass& getOuter() { return *(OuterClass*) \
	((intptr_t(this) + 1) - intptr_t(&((OuterClass*)1)->ThisVar)); } \
	const OuterClass& getOuter() const { return *(OuterClass*) \
	((intptr_t(this) + 1) - intptr_t(&((OuterClass*)1)->ThisVar)); } \
	OuterClass* getOuterSafe() { return this ? &getOuter() : NULL; } \
	const OuterClass* getOuterSafe() const { return this ? &getOuter() : NULL; }

class Noncopyable
{
protected:
	Noncopyable() {}
	~Noncopyable() {}

private:
	// Emphasize the following members are private
	Noncopyable(const Noncopyable&);
	const Noncopyable& operator=(const Noncopyable&);
};	// Noncopyable

#endif
