#ifndef __roCpu_h__
#define __roCpu_h__

#include "roCompiler.h"

#if roCPU_x86_64 + roCPU_x86 + roCPU_PowerPC + roCPU_ARM!= 1
#	error CPU should be specified
#endif

#if roCPU_x86
#	define roCPU_LP32			1
#	define roCPU_LITTLE_ENDIAN	1
#	define roCPU_SUPPORT_MEMORY_MISALIGNED	64	
#endif

#if roCPU_x86_64
#	define roCPU_LP64			1
#	define roCPU_LITTLE_ENDIAN	1
#	define roCPU_SUPPORT_MEMORY_MISALIGNED	64	
#endif

#if roCPU_PowerPC
#	define roCPU_LP32			1
#	define roCPU_BIG_ENDIAN	1
#	define roCPU_SUPPORT_MEMORY_MISALIGNED	64	
#endif

#if roCPU_ARM
#	define roCPU_LP32			1
#	define roCPU_SUPPORT_MEMORY_MISALIGNED	8
#endif

#if roCPU_LP32 + roCPU_LP64 != 1
#	error CPU bits should be specified
#endif

#if roCPU_BIG_ENDIAN + roCPU_LITTLE_ENDIAN != 1
#	error CPU uint8_t order should be specified
#endif

#endif	// __roCpu_h__
