#include "socket_option.hpp"

#ifdef _WIN32
	#ifndef WIN32_LEAN_AND_MEAN
		#define WIN32_LEAN_AND_MEAN
	#endif
	#ifndef NOMINMAX
		#define NOMINMAX
	#endif
	#include <winsock2.h>
#else
	#include <sys/socket.h>
#endif

#define DEFINE_SOCKET_OPTION(L, name, O)  \
	int so_##name ::level  () { return L; } \
	int so_##name ::option () { return O; }

DEFINE_SOCKET_OPTION(SOL_SOCKET, broadcast, SO_BROADCAST)
DEFINE_SOCKET_OPTION(SOL_SOCKET, reuseaddr, SO_REUSEADDR)
DEFINE_SOCKET_OPTION(SOL_SOCKET, dontroute, SO_DONTROUTE)
DEFINE_SOCKET_OPTION(SOL_SOCKET, rcvbuf,    SO_RCVBUF)
DEFINE_SOCKET_OPTION(SOL_SOCKET, sndbuf,    SO_SNDBUF)
DEFINE_SOCKET_OPTION(SOL_SOCKET, error,     SO_ERROR)
