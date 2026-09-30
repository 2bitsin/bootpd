#pragma once

enum so_bool: int
{
	so_false = 0,
	so_true  = 1
};

template <typename Value_type>
struct socket_option_base
{
	using value_type = Value_type;
};

// Only options with an identical representation on Winsock and POSIX.
#define DEFINE_SOCKET_OPTION(name, type) \
	struct so_##name: socket_option_base<type> \
	{ static int level(); static int option(); };

DEFINE_SOCKET_OPTION(broadcast, so_bool)
DEFINE_SOCKET_OPTION(reuseaddr, so_bool)
DEFINE_SOCKET_OPTION(dontroute, so_bool)
DEFINE_SOCKET_OPTION(rcvbuf,    int)
DEFINE_SOCKET_OPTION(sndbuf,    int)
DEFINE_SOCKET_OPTION(error,     int)

#undef DEFINE_SOCKET_OPTION
