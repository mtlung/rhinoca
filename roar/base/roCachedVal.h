#ifndef __roCachedVal_h__
#define __roCachedVal_h__

#include "../platform/roCompiler.h"

namespace ro {

/// Reference: http://stackoverflow.com/questions/4936987/c-class-member-variable-knowing-its-own-offset
template<class Owner, class T, class Tag, void(Owner::*update)()>
struct _CachedMemberVal
{
	_CachedMemberVal() : isDirty(true), val() {}
	_CachedMemberVal(const _CachedMemberVal& rhs) : isDirty(true), val() {}

	operator const T&() const
	{
		return *operator->();
	}

	const T* operator->()
	{
		if(isDirty) {
			(owner()->*update)();
			isDirty = false;
		}
		return &val;
	}

	void markDirty() {
		isDirty = true;
	}

    Owner* owner()
    {
        return reinterpret_cast<Owner*>(
            reinterpret_cast<char*>(this) - Tag::offset());
    }

	mutable bool isDirty;
	T val;
};

}	// namespace

/// Usage: roCachedMemberVal(Canvas, Mat4, viewProj, &Canvas::_updateViewProj);
#define roCachedMemberVal(Owner, T, name, updater) \
struct _name ## _tag { \
    static roPtrInt offset() { \
        return roOffsetof(Owner, name); \
    } \
}; \
_CachedMemberVal<Owner, T, _name ## _tag, updater> name

#endif	// __roCachedVal_h__
