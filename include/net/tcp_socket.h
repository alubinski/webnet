#pragma once
#include "detail/socket.h"
#include "detail/socket_flags.h"
#include "detail/socket_handle.h"
#include "endpoint.h"
#include "net/detail/platform_error.h"
#include <stdexcept>
#include <system_error>

namespace net {

/**
 * @brief High-level TCP socket wrapper.
 *
 * Provides a type-safe interface for TCP stream sockets on top of
 * the platform-independent base `Socket` class. Supports connect,
 * bind, listen, accept, send, and receive operations.
 */
class TcpSocket : public detail::Socket {

public:
  using AddressFamily = detail::SocketFlags::AddressFamily;
  using BlockingType = detail::SocketFlags::BlockingType;
  using InheritableType = detail::SocketFlags::InheritableType;
  using ShutdownType = detail::SocketFlags::ShutdownType;
  using SocketType = detail::SocketFlags::SocketType;
  using ProtocolType = detail::SocketFlags::ProtocolType;
  using Socket = detail::Socket;
  using Handle = detail::SocketDescriptorHandle::Handle;

  /**
   * @brief Construct a TCP socket.
   *
   * Creates an unconnected TCP stream socket.
   *
   * @param address_family IPv4/IPv6
   * @param blocking       Blocking or non-blocking
   * @param inheritable    Whether the handle is inheritable
   */
  explicit TcpSocket(AddressFamily family = AddressFamily::IPV4,
                     BlockingType blocking = BlockingType::Blocking,
                     InheritableType inheritable = InheritableType::Inheritable)
      : Socket(family, SocketType::Stream, ProtocolType::TCP, blocking,
               inheritable) {}

  TcpSocket(TcpSocket &&other) noexcept = default;
  TcpSocket &operator=(TcpSocket &&other) noexcept = default;

  TcpSocket(const TcpSocket &) = delete;
  TcpSocket &operator=(const TcpSocket &) = delete;

  /**
   * @brief Destructor closes the socket if valid.
   */
  ~TcpSocket() = default;

  /**
   * @brief Connect to a remote endpoint.
   *
   * @param ep Remote endpoint (IP + port).
   *
   * @throws std::system_error if the connection fails.
   */
  void connect(const Endpoint &ep);

  /**
   * @brief Bind socket to a local endpoint.
   *
   * @param ep Local endpoint (IP + port).
   *
   * @throws std::system_error on failure.
   */
  void bind(const Endpoint &ep);

  /**
   * @brief Enable or disable SO_REUSEADDR.
   *
   * @param enable True to allow reuse, false to disable.
   */
  void setReuseAddress(bool enable);

  /**
   * @brief Start listening for incoming connections.
   *
   * @param backlog Maximum number of pending connections (default: SOMAXCONN).
   *
   * @throws std::system_error on failure.
   */
  void listen(int backlog = SOMAXCONN);

  /**
   * @brief Accept an incoming connection.
   *
   * @param peer Endpoint structure to receive the peer address.
   *
   * @return TcpSocket representing the accepted connection.
   *
   * @throws std::system_error if accept fails.
   *
   * @note Returned socket inherits blocking and inheritable flags from this
   * socket.
   */
  [[nodiscard]] TcpSocket accept(Endpoint &peer);

  /**
   * @brief Send bytes over the connection.
   *
   * @param data Buffer containing bytes to send.
   *
   * @return Number of bytes actually sent (may be less than buffer size).
   *
   * @throws std::system_error on failure.
   */
  [[nodiscard]]
  std::size_t send(std::span<const std::byte> data);

  /**
   * @brief Receive bytes from the connection.
   *
   * @param buffer Buffer to store received bytes.
   *
   * @return Number of bytes received. Returns 0 if peer closed the connection.
   *
   * @throws std::system_error on failure.
   */
  [[nodiscard]]
  std::size_t receive(std::span<std::byte> buffer);

  /**
   * @brief Retrieve the local endpoint the socket is bound to.
   *
   * @return Endpoint representing the local address and port.
   *
   * @throws std::logic_error if socket is invalid.
   * @throws std::system_error if getsockname fails.
   */
  Endpoint localEndpoint() const;

private:
  /**
   * @brief Internal constructor used by accept().
   *
   * Creates a TcpSocket from an existing handle.
   *
   * @param handle Socket handle returned by accept().
   * @param family Address family (IPv4/IPv6).
   * @param blocking Blocking mode.
   * @param inheritable Handle inheritable flag.
   */
  TcpSocket(Handle handle, AddressFamily family, BlockingType blocking,
            InheritableType inheritable)
      : Socket(handle, family, SocketType::Stream, ProtocolType::TCP, blocking,
               inheritable) {}
};

} // namespace net
