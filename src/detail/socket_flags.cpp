#include "net/detail/socket_flags.h"
#include <utility>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <netdb.h>
#endif

namespace net::detail {

int SocketFlags::toNative(AddressFamily af) {
  switch (af) {
  case AddressFamily::IPV4:
    return AF_INET;
  case AddressFamily::IPV6:
    return AF_INET6;
  }
  std::unreachable();
}

int SocketFlags::toNative(SocketType st) {
  switch (st) {
  case SocketType::Stream:
    return SOCK_STREAM;
  case SocketType::Dgram:
    return SOCK_DGRAM;
  }
  std::unreachable();
}

int SocketFlags::toNative(ProtocolType pt) {
  switch (pt) {
  case ProtocolType::TCP:
    return IPPROTO_TCP;
  case ProtocolType::UDP:
    return IPPROTO_UDP;
  }
  std::unreachable();
}

int SocketFlags::toNative(ShutdownType st) {
#ifdef _WIN32
  switch (st) {
  case ShutdownType::Receiving:
    return SD_RECEIVE;
  case ShutdownType::Sending:
    return SD_SEND;
  case ShutdownType::Both:
    return SD_BOTH;
  }
#else
  switch (st) {
  case ShutdownType::Receiving:
    return SHUT_RD;
  case ShutdownType::Sending:
    return SHUT_WR;
  case ShutdownType::Both:
    return SHUT_RDWR;
  }
#endif
  std::unreachable();
}

} // namespace net::detail
