#include "net/core/endpoint.h"
#include <cstring>
#include <stdexcept>

#ifndef _WIN32
#include <arpa/inet.h>
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

namespace net {

Endpoint::Endpoint(const detail::IpAddress &address, std::uint16_t port) {
  std::memset(&storage_, 0, sizeof(storage_));

  if (address.type() == detail::IpAddress::Type::IPv4) {
    auto *addr = reinterpret_cast<sockaddr_in *>(&storage_);
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
    std::memcpy(&addr->sin_addr, address.data(), sizeof(in_addr));

    size_ = sizeof(sockaddr_in);
  } else {
    auto *addr = reinterpret_cast<sockaddr_in6 *>(&storage_);
    addr->sin6_family = AF_INET6;
    addr->sin6_port = htons(port);
    std::memcpy(&addr->sin6_addr, address.data(), sizeof(in6_addr));

    size_ = sizeof(sockaddr_in6);
  }
}

Endpoint::Endpoint(const std::string &address, std::uint16_t port)
    : Endpoint(detail::IpAddress(address), port) {}

std::uint16_t Endpoint::port() const noexcept {
  if (storage_.ss_family == AF_INET) {
    const auto *addr = reinterpret_cast<const sockaddr_in *>(&storage_);
    return ntohs(addr->sin_port);
  } else if (storage_.ss_family == AF_INET6) {
    const auto *addr = reinterpret_cast<const sockaddr_in6 *>(&storage_);
    return ntohs(addr->sin6_port);
  }

  return 0;
}

std::string Endpoint::to_string() const {
  char buffer[INET6_ADDRSTRLEN] = {};

  if (storage_.ss_family == AF_INET) {
    const auto *addr = reinterpret_cast<const sockaddr_in *>(&storage_);

    if (::inet_ntop(AF_INET, &addr->sin_addr, buffer, sizeof(buffer)) ==
        nullptr) {
      throw std::runtime_error("inet_ntop failed");
    }

    return std::string(buffer) + ":" + std::to_string(ntohs(addr->sin_port));
  }

  if (storage_.ss_family == AF_INET6) {
    const auto *addr = reinterpret_cast<const sockaddr_in6 *>(&storage_);

    if (::inet_ntop(AF_INET6, &addr->sin6_addr, buffer, sizeof(buffer)) ==
        nullptr) {
      throw std::runtime_error("inet_ntop failed");
    }

    return "[" + std::string(buffer) +
           "]:" + std::to_string(ntohs(addr->sin6_port));
  }

  throw std::logic_error("Unknown address family in Endpoint");
}

} // namespace net
