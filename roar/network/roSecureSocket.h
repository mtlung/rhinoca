#ifndef __network_roSecureSocket_h__
#define __network_roSecureSocket_h__

#include "roSocket.h"

namespace ro {

struct SecureSocket
{
	SecureSocket();
	~SecureSocket();

	roStatus create();
	roStatus connect(const SockAddr& endPoint);

	CoSocket _socket;
};	// SecureSocket

}	// namespace ro

#endif	// __network_roSecureSocket_h__
