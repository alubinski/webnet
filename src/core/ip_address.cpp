#include "net/detail/ip_address.h"
#include <cstring>
#include <stdexcept>
#ifndef _WIN32
#include <arpa/inet.h>
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#endif // !_WIN32

namespace net::detail {

IpAddress::IpAddress(const std::string &address) {
  // Try IPv4 first
  if (::inet_pton(AF_INET, address.c_str(), &storage_.v4) == 1) {
    type_ = Type::IPv4;
    return;
  }

  // Try IPv6
  if (::inet_pton(AF_INET6, address.c_str(), &storage_.v6) == 1) {
    type_ = Type::IPv6;
    return;
  }

  throw std::invalid_argument("Invalid IP address format: " + address);
}

IpAddress::IpAddress(const void *bytes, Type type) : type_(type) {
  if (!bytes) {
    throw std::invalid_argument("IpAddress bytes pointer is null");
  }

  if (type_ == Type::IPv4) {
    std::memcpy(&storage_.v4, bytes, sizeof(in_addr));
  } else {
    std::memcpy(&storage_.v6, bytes, sizeof(in6_addr));
  }
}

int IpAddress::family() const noexcept {
  return (type_ == Type::IPv4) ? AF_INET : AF_INET6;
}

const void *IpAddress::data() const noexcept {
  return (type_ == Type::IPv4) ? static_cast<const void *>(&storage_.v4)
                               : static_cast<const void *>(&storage_.v6);
}

std::string IpAddress::to_string() const {
  char buffer[INET6_ADDRSTRLEN] = {};

  const void *src = data();

  if (::inet_ntop(family(), src, buffer, sizeof(buffer)) == nullptr) {
    throw std::runtime_error("inet_ntop failed");
  }

  return std::string(buffer);
}
} // namespace net::detail
