#pragma once
#include "detail/ip_address.h"
#include "detail/platform_types.h"
#include <cstdint>

namespace net {

/**
 * @brief Represents a network endpoint (IP address + port).
 *
 * Encapsulates the platform-specific sockaddr_storage structure,
 * providing utilities for socket operations such as bind, connect,
 * and accept.
 */
class Endpoint {
public:
  /**
   * @brief Default constructor.
   *
   * Initializes an unspecified endpoint (zeroed storage and full size).
   */
  Endpoint() = default;

  /**
   * @brief Constructs an endpoint from an IpAddress and port.
   *
   * @param address The IP address.
   * @param port The port number in host byte order.
   */
  Endpoint(const detail::IpAddress &address, std::uint16_t port);

  /**
   * @brief Constructs an endpoint from a string address and port.
   *
   * @param address IP address string (IPv4 or IPv6).
   * @param port Port number in host byte order.
   *
   * @throws std::invalid_argument if the string is not a valid IP address.
   */
  Endpoint(const std::string &address, std::uint16_t port);

  /**
   * @brief Returns a const pointer to the underlying sockaddr.
   *
   * Can be used in system calls such as `bind`, `connect`, or `accept`.
   *
   * @return Pointer to sockaddr.
   */
  const sockaddr *data() const noexcept {
    return reinterpret_cast<const sockaddr *>(&storage_);
  }

  /**
   * @brief Returns a mutable pointer to the underlying sockaddr.
   *
   * Useful for system calls that fill in the address, e.g., `accept`.
   *
   * @return Pointer to sockaddr.
   */
  sockaddr *data() noexcept { return reinterpret_cast<sockaddr *>(&storage_); }

  /**
   * @brief Returns the size of the sockaddr_storage.
   *
   * @return Current size of the address structure.
   */
  detail::socket_length_t size() const noexcept { return size_; };

  /**
   * @brief Returns a pointer to the size of the sockaddr_storage.
   *
   * Useful for system calls that expect a pointer to length (e.g., `accept`).
   *
   * @return Pointer to size.
   */
  detail::socket_length_t *size_ptr() noexcept { return &size_; }

  /**
   * @brief Returns a pointer to the size of the sockaddr_storage.
   *
   * Useful for system calls that expect a pointer to length (e.g., `accept`).
   *
   * @return Pointer to size.
   */
  std::uint16_t port() const noexcept;

  /**
   * @brief Converts the endpoint to a human-readable string.
   *
   * Format: "IP:port" (e.g., "127.0.0.1:8080" or "[::1]:8080").
   *
   * @return String representation of the endpoint.
   */
  std::string to_string() const;

  /**
   * @brief Sets the size of the sockaddr_storage.
   *
   * Used after system calls that modify the address length, e.g., `accept`.
   *
   * @param size New size value.
   */
  void set_size(detail::socket_length_t size) noexcept { size_ = size; }

private:
  sockaddr_storage storage_; ///< Internal storage for the address
  detail::socket_length_t size_ =
      sizeof(storage_); ///< Current size of the address
};

} // namespace net
