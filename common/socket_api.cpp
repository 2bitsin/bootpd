#include <stdexcept>
#include <string>
#include <string_view>
#include <mutex>
#include <system_error>
#include <iostream>
#include <system_error>
#include <charconv>

#include <common/byte_order.hpp>

#include "socket_api.hpp"
#include "address_v4.hpp"
#include "socket_error.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <ws2tcpip.h>
#include <WinSock2.h>
#include <Windows.h>

#pragma comment(lib, "ws2_32.lib")


static auto socket_last_error() -> std::int32_t
{
  return WSAGetLastError();
}

static auto is_time_out_error(std::int32_t error)
{
  return WSAETIMEDOUT == error;
}

static auto last_error_as_string(std::int32_t last_error = socket_last_error()) -> std::string
{
  using namespace std::string_literals; 
  switch(last_error)
  {
  case WSAEINTR                         : return "WSAEINTR"s                    ;
  case WSAEBADF                         : return "WSAEBADF"s                    ;      
  case WSAEACCES                        : return "WSAEACCES"s                   ;      
  case WSAEFAULT                        : return "WSAEFAULT"s                   ;      
  case WSAEINVAL                        : return "WSAEINVAL"s                   ;      
  case WSAEMFILE                        : return "WSAEMFILE"s                   ;      
  case WSAEWOULDBLOCK                   : return "WSAEWOULDBLOCK"s              ;
  case WSAEINPROGRESS                   : return "WSAEINPROGRESS"s              ;      
  case WSAEALREADY                      : return "WSAEALREADY"s                 ;      
  case WSAENOTSOCK                      : return "WSAENOTSOCK"s                 ;      
  case WSAEDESTADDRREQ                  : return "WSAEDESTADDRREQ"s             ;      
  case WSAEMSGSIZE                      : return "WSAEMSGSIZE"s                 ;      
  case WSAEPROTOTYPE                    : return "WSAEPROTOTYPE"s               ;      
  case WSAENOPROTOOPT                   : return "WSAENOPROTOOPT"s              ;      
  case WSAEPROTONOSUPPORT               : return "WSAEPROTONOSUPPORT"s          ;      
  case WSAESOCKTNOSUPPORT               : return "WSAESOCKTNOSUPPORT"s          ;      
  case WSAEOPNOTSUPP                    : return "WSAEOPNOTSUPP"s               ;      
  case WSAEPFNOSUPPORT                  : return "WSAEPFNOSUPPORT"s             ;      
  case WSAEAFNOSUPPORT                  : return "WSAEAFNOSUPPORT"s             ;      
  case WSAEADDRINUSE                    : return "WSAEADDRINUSE"s               ;      
  case WSAEADDRNOTAVAIL                 : return "WSAEADDRNOTAVAIL"s            ;      
  case WSAENETDOWN                      : return "WSAENETDOWN"s                 ;      
  case WSAENETUNREACH                   : return "WSAENETUNREACH"s              ;      
  case WSAENETRESET                     : return "WSAENETRESET"s                ;      
  case WSAECONNABORTED                  : return "WSAECONNABORTED"s             ;      
  case WSAECONNRESET                    : return "WSAECONNRESET"s               ;      
  case WSAENOBUFS                       : return "WSAENOBUFS"s                  ;      
  case WSAEISCONN                       : return "WSAEISCONN"s                  ;      
  case WSAENOTCONN                      : return "WSAENOTCONN"s                 ;      
  case WSAESHUTDOWN                     : return "WSAESHUTDOWN"s                ;      
  case WSAETOOMANYREFS                  : return "WSAETOOMANYREFS"s             ;      
  case WSAETIMEDOUT                     : return "WSAETIMEDOUT"s                ;      
  case WSAECONNREFUSED                  : return "WSAECONNREFUSED"s             ;      
  case WSAELOOP                         : return "WSAELOOP"s                    ;      
  case WSAENAMETOOLONG                  : return "WSAENAMETOOLONG"s             ;      
  case WSAEHOSTDOWN                     : return "WSAEHOSTDOWN"s                ;      
  case WSAEHOSTUNREACH                  : return "WSAEHOSTUNREACH"s             ;      
  case WSAENOTEMPTY                     : return "WSAENOTEMPTY"s                ;      
  case WSAEPROCLIM                      : return "WSAEPROCLIM"s                 ;      
  case WSAEUSERS                        : return "WSAEUSERS"s                   ;      
  case WSAEDQUOT                        : return "WSAEDQUOT"s                   ;      
  case WSAESTALE                        : return "WSAESTALE"s                   ;      
  case WSAEREMOTE                       : return "WSAEREMOTE"s                  ;      
  case WSASYSNOTREADY                   : return "WSASYSNOTREADY"s              ;      
  case WSAVERNOTSUPPORTED               : return "WSAVERNOTSUPPORTED"s          ;      
  case WSANOTINITIALISED                : return "WSANOTINITIALISED"s           ;      
  case WSAEDISCON                       : return "WSAEDISCON"s                  ;      
  case WSAENOMORE                       : return "WSAENOMORE"s                  ;      
  case WSAECANCELLED                    : return "WSAECANCELLED"s               ;      
  case WSAEINVALIDPROCTABLE             : return "WSAEINVALIDPROCTABLE"s        ;      
  case WSAEINVALIDPROVIDER              : return "WSAEINVALIDPROVIDER"s         ;      
  case WSAEPROVIDERFAILEDINIT           : return "WSAEPROVIDERFAILEDINIT"s      ;      
  case WSASYSCALLFAILURE                : return "WSASYSCALLFAILURE"s           ;      
  case WSASERVICE_NOT_FOUND             : return "WSASERVICE_NOT_FOUND"s        ;      
  case WSATYPE_NOT_FOUND                : return "WSATYPE_NOT_FOUND"s           ;      
  case WSA_E_NO_MORE                    : return "WSA_E_NO_MORE"s               ;      
  case WSA_E_CANCELLED                  : return "WSA_E_CANCELLED"s             ;      
  case WSAEREFUSED                      : return "WSAEREFUSED"s                 ;      
  case WSAHOST_NOT_FOUND                : return "WSAHOST_NOT_FOUND"s           ;      
  case WSATRY_AGAIN                     : return "WSATRY_AGAIN"s                ;      
  case WSANO_RECOVERY                   : return "WSANO_RECOVERY"s              ;      
  case WSANO_DATA                       : return "WSANO_DATA"s                  ;      
  case WSA_QOS_RECEIVERS                : return "WSA_QOS_RECEIVERS"s           ;      
  case WSA_QOS_SENDERS                  : return "WSA_QOS_SENDERS"s             ;      
  case WSA_QOS_NO_SENDERS               : return "WSA_QOS_NO_SENDERS"s          ;      
  case WSA_QOS_NO_RECEIVERS             : return "WSA_QOS_NO_RECEIVERS"s        ;      
  case WSA_QOS_REQUEST_CONFIRMED        : return "WSA_QOS_REQUEST_CONFIRMED"s   ;      
  case WSA_QOS_ADMISSION_FAILURE        : return "WSA_QOS_ADMISSION_FAILURE"s   ;      
  case WSA_QOS_POLICY_FAILURE           : return "WSA_QOS_POLICY_FAILURE"s      ;      
  case WSA_QOS_BAD_STYLE                : return "WSA_QOS_BAD_STYLE"s           ;      
  case WSA_QOS_BAD_OBJECT               : return "WSA_QOS_BAD_OBJECT"s          ;      
  case WSA_QOS_TRAFFIC_CTRL_ERROR       : return "WSA_QOS_TRAFFIC_CTRL_ERROR"s  ;      
  case WSA_QOS_GENERIC_ERROR            : return "WSA_QOS_GENERIC_ERROR"s       ;      
  case WSA_QOS_ESERVICETYPE             : return "WSA_QOS_ESERVICETYPE"s        ;      
  case WSA_QOS_EFLOWSPEC                : return "WSA_QOS_EFLOWSPEC"s           ;      
  case WSA_QOS_EPROVSPECBUF             : return "WSA_QOS_EPROVSPECBUF"s        ;      
  case WSA_QOS_EFILTERSTYLE             : return "WSA_QOS_EFILTERSTYLE"s        ;      
  case WSA_QOS_EFILTERTYPE              : return "WSA_QOS_EFILTERTYPE"s         ;      
  case WSA_QOS_EFILTERCOUNT             : return "WSA_QOS_EFILTERCOUNT"s        ;      
  case WSA_QOS_EOBJLENGTH               : return "WSA_QOS_EOBJLENGTH"s          ;      
  case WSA_QOS_EFLOWCOUNT               : return "WSA_QOS_EFLOWCOUNT"s          ;      
  case WSA_QOS_EUNKOWNPSOBJ             : return "WSA_QOS_EUNKOWNPSOBJ"s        ;      
  case WSA_QOS_EPOLICYOBJ               : return "WSA_QOS_EPOLICYOBJ"s          ;      
  case WSA_QOS_EFLOWDESC                : return "WSA_QOS_EFLOWDESC"s           ;      
  case WSA_QOS_EPSFLOWSPEC              : return "WSA_QOS_EPSFLOWSPEC"s         ;      
  case WSA_QOS_EPSFILTERSPEC            : return "WSA_QOS_EPSFILTERSPEC"s       ;      
  case WSA_QOS_ESDMODEOBJ               : return "WSA_QOS_ESDMODEOBJ"s          ;      
  case WSA_QOS_ESHAPERATEOBJ            : return "WSA_QOS_ESHAPERATEOBJ"s       ;      
  case WSA_QOS_RESERVED_PETYPE          : return "WSA_QOS_RESERVED_PETYPE"s     ;      
  case WSA_SECURE_HOST_NOT_FOUND        : return "WSA_SECURE_HOST_NOT_FOUND"s   ;      
  case WSA_IPSEC_NAME_POLICY_ERROR      : return "WSA_IPSEC_NAME_POLICY_ERROR"s ;
  default: break;
  }
  return "#"s + std::to_string(last_error);
}


static void v4_initialize()
{
  static std::once_flag _;
  static WSADATA wsad;
  std::call_once(_, []() 
  {
    using namespace std::string_literals;
    RtlSecureZeroMemory(&wsad, sizeof(wsad));
    if (auto wsaerr = WSAStartup(MAKEWORD(2, 2), &wsad); wsaerr != 0)
      throw std::runtime_error("WSAStartup failed with code : "s + last_error_as_string());
    std::atexit([]() 
    {
      if (auto wsaerr = WSACleanup(); wsaerr != 0) {
        std::cerr << ("WARNING! WSACleanup failed with code : "s + last_error_as_string() + "\n"s);
      }
    });
  });
}

auto v4_resolve_single(std::string_view target) -> std::uint32_t
{
  using namespace std::string_literals;
  std::string tmp{ target };

  v4_initialize();
  
  auto host_e = gethostbyname(tmp.c_str());
  if (!host_e || !host_e->h_addr_list[0])
    throw std::runtime_error(tmp + " cannot be resolved, error : "s + last_error_as_string());
  if (host_e->h_addrtype != AF_INET)
    throw std::runtime_error(tmp + " cannot be resolved to a v4 address."s);
  return net_to_host(*(const uint32_t*)host_e->h_addr_list[0]);
}


auto v4_socket_make_udp() -> int_socket_type
{
  using namespace std::string_literals;

  v4_initialize ();


  if (auto int_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP); int_sock != INVALID_SOCKET)
  {
    return int_sock;  
  }
  throw std::runtime_error("can't create socket, error code : "s + 
                           last_error_as_string());   
}

auto v4_socket_make_udp(const address_v4& address) -> int_socket_type
{
  const auto int_sock = v4_socket_make_udp();
  v4_socket_bind(int_sock, address);
  return int_sock;
}

auto v4_socket_make_invalid() -> int_socket_type
{
  return INVALID_SOCKET;
}

void v4_socket_bind(int_socket_type socket, const address_v4& address)
{ 
  using namespace std::string_literals;

  v4_initialize();

  const auto sai = address.as<sockaddr_in>();
  if (const auto error = bind(socket, (const sockaddr*)&sai, sizeof(sai)); error != 0)
    throw std::runtime_error(std::format("failed to bind socket with address '{}', error code : {}", address.to_string(), last_error_as_string()));
}

void v4_init_sockaddr(sockaddr_in& target, std::size_t len, const struct address_v4& source)
{
  RtlSecureZeroMemory(&target, len);
  target.sin_family = AF_INET;
  target.sin_port = source.net_port();
  target.sin_addr.S_un.S_addr = source.net_addr();
}

void v4_init_sockaddr(sockaddr& target, std::size_t len, const struct address_v4& source)
{
  if(len < sizeof(sockaddr_in))
    throw std::logic_error("`target` is too small.");
  v4_init_sockaddr(*reinterpret_cast<sockaddr_in*>(&target), len, source);
}

auto v4_parse_address_and_port(std::string_view what) -> std::pair<uint32_t, uint16_t>
{ 
  using namespace std::string_literals;
  uint32_t port{ 0 };
  in_addr address;
  std::string tmp;
    
  v4_initialize();
  
  if (auto it = what.find(':'); it != what.npos) {
    tmp = what.substr(it + 1);
    if (tmp.size() > 0)
    {
      std::size_t idx;
      port = std::stoul(tmp, &idx, 10);
      if (idx != tmp.size() || port > 65535)
        throw std::logic_error(tmp + " is not a valid port number.");
      port &= 0xffff;
    }
    what = what.substr(0, it);
  }   
  tmp = what;
  if (!inet_pton(AF_INET, tmp.c_str(), &address)) {
    return { v4_resolve_single(tmp), port };
  }
  return { net_to_host(address.S_un.S_addr), port };
}

auto v4_parse_address_and_port(sockaddr_in const& what) -> std::pair<uint32_t, uint16_t>
{
  if (what.sin_family != AF_INET)
    throw std::logic_error("address family mismatch.");
  return std::pair {
    net_to_host(what.sin_addr.S_un.S_addr),
    net_to_host(what.sin_port)
  };
}

auto v4_address_to_string(std::uint32_t address) -> std::string
{
  using namespace std::string_literals;
  char buff [2048];
  in_addr addr_bits;  

  v4_initialize();
  
  std::memset(buff, 0, sizeof(buff));
  addr_bits.S_un.S_addr = host_to_net(address);
  if (!inet_ntop(AF_INET, &addr_bits, buff, sizeof(buff)))
    throw std::runtime_error("unable to convert address to string, error code : "s + 
                             last_error_as_string());
  std::string tmp;
  tmp.assign(buff);
  return tmp;
}

auto v4_parse_address(std::string_view what) -> uint32_t
{
  auto[address, port] = v4_parse_address_and_port(what);
  return address;
}

auto v4_socket_close(int_socket_type socket) -> void
{
  using namespace std::string_literals;

  v4_initialize();

  if (const auto error = closesocket(socket); error != 0)
  {
    std::cerr << ("WARNING! failed to close socket, error code : "s + last_error_as_string() + "\n"s);
  }
}



auto v4_socket_recv(int_socket_type socket, std::span<std::byte>& buffer, address_v4& address, std::uint32_t flags) -> std::size_t
{
  using namespace std::string_literals;

  sockaddr_in addr_in;
  int addr_len{ sizeof(addr_in) };
  const auto size = std::min((int)buffer.size(), 0x7fffffff);
  auto* const data = (char*)buffer.data(); 

  v4_initialize();

  auto received_bytes = recvfrom(socket, data, size, (int)flags, (sockaddr*)&addr_in, &addr_len);
  if (received_bytes >= 0) 
  {
    if (addr_len != sizeof (addr_in))
      throw std::runtime_error("packet sender address size mismatch."s);
    address.assign_from(addr_in);
    buffer = buffer.subspan(0, (unsigned)received_bytes);
    return received_bytes;
  }

  if (const auto error_code = socket_last_error(); is_time_out_error(error_code))
    throw  error_socket_timed_out{ "receive operation timed out." };

  throw std::runtime_error("failed to receive bytes from socket, error code : "s + 
                           last_error_as_string());
}

auto v4_socket_send(int_socket_type socket, std::span<const std::byte>& buffer, const address_v4& address, std::uint32_t flags) -> std::size_t
{
  using namespace std::string_literals;
  const auto size = std::min((int)buffer.size(), 0x7fffffff);
  auto* const data = (char*)buffer.data(); 
  const auto addr_in = address.as<sockaddr_in>();

  v4_initialize();

  auto sent_bytes = sendto(socket, data, size, (int)flags, (const sockaddr*)&addr_in, sizeof(addr_in));
  if (sent_bytes >= 0)
  {
    buffer = buffer.subspan(sent_bytes);
    return sent_bytes;
  }

  if (const auto error_code = socket_last_error(); is_time_out_error(error_code))
    throw error_socket_timed_out("send operation timed out.");

  throw std::runtime_error("failed to send bytes trough socket, error code : "s + 
                           last_error_as_string());
}

static auto to_hex(std::uint8_t value) -> std::string
{
  static constexpr const char x [] = "0123456789ABCDEF";
  return std::string{ x[(value >> 4) & 0xf], x[value & 0xf] };
}

auto mac_address_to_string(std::span<const std::uint8_t> data)
  -> std::string
{
  using namespace std::string_literals;
  if (data.size () < 1u)
    throw std::runtime_error("address is empty"s);
  
  std::string value;  
  value.append(to_hex(data.front()));
  for (auto&& a_byte : data.subspan(1u))
  {
    value.push_back('-');
    value.append(to_hex(a_byte));
  }
  return value;
}

namespace detail
{
  void socket_option_set(int_socket_type target, int level, int option, const void* value, int size)
  {
    using namespace std::string_literals;
    if (setsockopt(target, level, option, (const char*)value, size) != 0)
      throw std::runtime_error("failed to set socket option, error code : "s + 
                              last_error_as_string());
  }
  
  void socket_option_get(int_socket_type target, int level, int option, void* value, int size)
  {
    using namespace std::string_literals;
    auto expected_size = size;
    if (getsockopt(target, level, option, (char*)value, &size) != 0)
      throw std::runtime_error("failed to get socket option, error code : "s + 
                              last_error_as_string());
    if (size != expected_size)
      throw std::logic_error("socket option value size mismatch."s);
  }
}
