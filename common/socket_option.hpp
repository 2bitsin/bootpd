#pragma once

#include <cstdint>
#include <cstddef>
#include <type_traits>
#include <tuple>
#include <ctime>

enum so_bool: int
{
	so_false = 0,
	so_true  = 1
};

enum so_sock_type: int
{
	so_sock_stream,
	so_sock_dgram
};

template <typename... Value_type>
struct socket_option_base 
{
	using tuple_type = std::tuple<Value_type...>;
	using value_type = std::conditional_t<(sizeof...(Value_type) == 1u),  std::tuple_element_t<0u, tuple_type>, tuple_type> ;
};

#define DEFINE_SOCKET_OPTION(name, ...) \
	struct so_##name: socket_option_base<__VA_ARGS__> \
	{ static int level(); static int option(); };

DEFINE_SOCKET_OPTION(broadcast,						so_bool)
DEFINE_SOCKET_OPTION(conditional_accept,	so_bool)
DEFINE_SOCKET_OPTION(debug,								so_bool)
DEFINE_SOCKET_OPTION(dontlinger,					so_bool)
DEFINE_SOCKET_OPTION(dontroute,						so_bool)
DEFINE_SOCKET_OPTION(error,								int32_t)
DEFINE_SOCKET_OPTION(group_priority,			int32_t)
DEFINE_SOCKET_OPTION(keepalive,						so_bool)
DEFINE_SOCKET_OPTION(linger,							uint16_t, uint16_t)
DEFINE_SOCKET_OPTION(oobinline,						so_bool)
DEFINE_SOCKET_OPTION(rcvbuf,							int32_t)
DEFINE_SOCKET_OPTION(reuseaddr,						so_bool)
DEFINE_SOCKET_OPTION(exclusiveaddruse,		so_bool)
DEFINE_SOCKET_OPTION(rcvtimeo,						uint32_t)
DEFINE_SOCKET_OPTION(sndtimeo,						uint32_t)
DEFINE_SOCKET_OPTION(sndbuf,							int32_t)				
DEFINE_SOCKET_OPTION(acceptconn,					so_bool)
DEFINE_SOCKET_OPTION(rcvlowat,						int32_t)
DEFINE_SOCKET_OPTION(sndlowat,						int32_t)
DEFINE_SOCKET_OPTION(type,								so_sock_type)

#undef DEFINE_SOCKET_OPTION


		



	


