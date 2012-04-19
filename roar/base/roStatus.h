#ifndef __roStatus_h__
#define __roStatus_h__

#include "../platform/roCompiler.h"

// Some marcos to simplify error handling
#define roEXCP_TRY bool _roExcp_has_throw = false; do {
#define roEXCP_THROW {_roExcp_has_throw = true; goto roEXCP_CATCH_LABEL;}
#define roEXCP_CATCH } while(false); roEXCP_CATCH_LABEL:if(_roExcp_has_throw) {
#define roEXCP_END }

struct roStatus
{
	enum Code {
		ok = 0,
#define roStatusEnum(n) n,
		_std_start = 8000,	// Standard error -9000 ~ -8001
#include "roStatusEnum.h"
		_std_end = 9001,
#undef roStatusEnum
	};

	roStatus()						{ _code = undefined; }
	roStatus(const roStatus& rhs)	{ _code = rhs._code; }
	roStatus(Code n)				{ _code = n; }

	operator	bool() const	{ return _code <= 0; }
	const char*	c_str() const;

	bool operator==(Code c) const{ return _code == c; }
	bool operator!=(Code c) const{ return _code != c; }

	Code _code;
	operator int()  const;
};

inline const char* roStatus::c_str() const
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

namespace ro {

typedef roStatus Status;

}	// namespace ro

#endif	// __roStatus_h__
