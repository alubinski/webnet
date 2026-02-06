#pragma once

#include <cstdint>
namespace net::detail {

/**
 * @brief Represents configuration flags for a socket.
 *
 * Stores all the key properties of a socket, such as its protocol,
 * type, address family, blocking mode, and inheritance behavior.
 * These flags are set at socket construction and remain constant
 * for the lifetime of the socket.
 */
struct SocketFlags {

  /**
   * @brief Specifies the IP address family used by the socket.
   *
   * Determines whether the socket communicates using IPv4 or IPv6.
   */
  enum class AddressFamily : std::uint8_t {
    IPV4, ///< Internet Protocol version 4 (AF_INET)
    IPV6  ///< Internet Protocol version 6 (AF_INET6)
  };

  /**
   * @brief Defines the communication semantics of the socket.
   *
   * Determines how data is transmitted between endpoints:
   */
  enum class SocketType : std::uint8_t {
    Stream, ///< Connection-oriented, reliable, ordered byte stream (e.g. TCP)
    Dgram   //< Connectionless, message-oriented communication (e.g. UDP)
  };

  /**
   * @brief Specifies the transport-layer protocol.
   *
   * Determines which protocol implementation is used by the socket.
   */
  enum class ProtocolType : std::uint8_t {
    TCP, ///< Transmission Control Protocol
    UDP  ///< User Datagram Protocol
  };

  /**
   * @brief Determines the blocking behavior of the socket.
   *
   * Controls whether socket operations wait for completion or return
   * immediately if the operation would block.
   */
  enum class BlockingType : std::uint8_t {
    NonBlocking, ///< Operations block until completion or error
    Blocking     ///< Operations return immediately if they would block
  };

  /**
   * @brief Controls whether the socket handle can be inherited by child
   * processes.
   *
   * Relevant primarily in multi-process environments.
   */
  enum class InheritableType : std::uint8_t {
    NonInheritable, ///< Handle is not inheritable
    Inheritable     ///< Handle is inheritable by child processes
  };

  /**
   * @brief Specifies which communication direction to disable via shutdown().
   *
   * Used to gracefully terminate part or all of a full-duplex connection.
   */
  enum class ShutdownType : std::uint8_t {
    Sending,   ///< Disable further send operations (SHUT_WR)
    Receiving, ///< Disable further receive operations (SHUT_RD)
    Both       ///< Disable both sending and receiving (SHUT_RDWR)
  };

  [[nodiscard]] static int toNative(AddressFamily);
  [[nodiscard]] static int toNative(SocketType);
  [[nodiscard]] static int toNative(ProtocolType);
  [[nodiscard]] static int toNative(ShutdownType);
};

} // namespace net::detail
