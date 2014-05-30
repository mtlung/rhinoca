#ifndef __network_roDnsResolve_h__
#define __network_roDnsResolve_h__

#include "../base/roStatus.h"

roStatus roGetHostByName(const roUtf8* hostname, roUint32& ipv4, float timeout=5.f);

#endif	// __network_roDnsResolve_h__
