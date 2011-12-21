#ifndef __roStatus_h__
#define __roStatus_h__

#include "../platform/roCompiler.h"

namespace ro {

struct Status
{
	Status()					{ _code = undefined; }
	Status(const Status& rhs)	{ _code = rhs._code; }
	Status(int n)				{ _code = n; }

	operator	bool() const	{ return _code >= 0; }
	int			code() const	{ return _code; }
	const char*	c_str() const;

	enum {
		ok = 0,
#define roStatusEnum(n) n,
		_std_start = -9000,	// Standard error -9000 ~ -8001
#include "roStatusEnum.h"
		_std_end = -8001,
#undef roStatusEnum
	};

private:
	int _code;
	operator int()  const;
};

inline const char* Status::c_str() const
{
	if(_code == 0) return "success";
	if(_code > _std_start && _code < _std_end) {
		switch(_code) {
#define roStatusEnum(n) case n: return #n;
#include "roStatusEnum.h"
#undef roStatusEnum
		}
	}
	return "unknown";
}

}	// namespace ro

#endif	// __roStatus_h__
