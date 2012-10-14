#ifndef __roComPtr_h__
#define __roComPtr_h__

#include "roUtility.h"
#include <atlbase.h>	// For CComPtr

// Reference: http://blogs.msdn.com/b/jaredpar/archive/2008/02/22/multiple-paths-to-iunknown.aspx?Redirected=true
template <class T>
class roComPtr : public CComPtrBase<T>
{
public:
	roComPtr() throw() {}
	roComPtr(int nNull) throw() : CComPtrBase<T>(nNull) {}
	roComPtr(T* lp) throw() : CComPtrBase<T>(lp) {}
	roComPtr(_In_ const roComPtr<T>& lp) throw() : CComPtrBase<T>(lp.p) {}

	T* operator=(_In_opt_ T* lp) throw()
	{
		if(*this != lp) {
			roComPtr<T> sp(lp);
			roSwap(p, sp.p);
		}
		return *this;
	}

	T* operator=(_In_ const roComPtr<T>& lp) throw()
	{
		if(*this != lp) {
			roComPtr<T> sp(lp);
			roSwap(p, sp.p);
		}
		return *this;
	}
};

#endif	// __roComPtr_h__
