#include "pch.h"
#include "roStatus.h"

const char* roStatus::c_str() const
{
	switch(_code) {
	case ok: return "ok";
#define roStatusEnum(n) case n: return #n;
#include "roStatusEnum.h"
#undef roStatusEnum
default: return "unknown";
	}
	return "unknown";
}
