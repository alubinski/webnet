#pragma once
#include "net/detail/platform_types.h"
#include <string>

#ifndef _WIN32
#include <netinet/in.h> // sockaddr_in
#include <sys/select.h> // select
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

namespace net::detail {

/**
 * @brief Represents an IPv4 or IPv6 network address.
 *
 * Provides constructors from string or raw data, and utilities
 * to query the address type, family, or to convert to string.
 */
class IpAddress {
public:
  /// Type of IP address: IPv4 or IPv6
  enum class Type { IPv4, IPv6 };

  /**
   * @brief Default constructor.
   *
   * Initializes to an unspecified IPv4 address (0.0.0.0).
   */
  IpAddress() = default;

  /**
   * @brief Constructs an IP address from a string.
   *
   * The string can be in IPv4 (e.g., "127.0.0.1") or IPv6 (e.g., "::1") format.
   *
   * @param address The IP address string.
   *
   * @throws std::invalid_argument if the string is not a valid IP address.
   */
  explicit IpAddress(const std::string &address);

  /**
   * @brief Constructs an IP address from raw binary data.
   *
   * Useful when reading from a socket or network packet.
   *
   * @param data Pointer to the address bytes (must match the Type).
   * @param type The type of the address (IPv4 or IPv6).
   */
  IpAddress(const void *data, Type type);

  /**
   * @brief Returns the address family constant suitable for socket calls.
   *
   * @return AF_INET for IPv4, AF_INET6 for IPv6.
   */
  int family() const noexcept;

  /**
   * @brief Converts the IP address to a human-readable string.
   *
   * @return IPv4 or IPv6 string representation.
   */
  std::string to_string() const;

  /**
   * @brief Returns a pointer to the raw address bytes.
   *
   * Can be used in socket functions such as `connect` or `bind`.
   *
   * @return Pointer to the address storage.
   */
  const void *data() const noexcept;

  /**
   * @brief Returns the type of the address.
   *
   * @return Type::IPv4 or Type::IPv6
   */
  Type type() const noexcept { return type_; }

private:
  Type type_{Type::IPv4}; ///< address type

  /// Internal storage for IPv4 or IPv6 address
  union {
    struct in_addr v4;  ///< IPv4 address
    struct in6_addr v6; ///< IPv6 address
  } storage_{};
};

} // namespace net::detail
