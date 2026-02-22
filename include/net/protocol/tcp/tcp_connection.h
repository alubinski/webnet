#pragma once
#include "net/connection/iconnection.h"
#include "net/core/endpoint.h"
#include "net/detail/socket_handle.h"
#include "tcp_socket.h"
#include <algorithm>
#include <coroutine>
#include <vector>

namespace net {

/**
 * @brief Represents a TCP connection implementing the IConnection interface.
 *
 * TcpConnection manages asynchronous read, write, and connect operations
 * over a TCP socket using coroutine-based suspension and notification.
 */
class TcpConnection : public IConnection {
public:
  using Handle = detail::SocketDescriptorHandle::Handle;

  /**
   * @brief Constructs a TcpConnection instance.
   *
   * Initializes the connection with an existing TcpSocket and remote endpoint.
   *
   * @param socket Underlying TCP socket to manage.
   * @param remote Remote endpoint associated with the connection.
   */
  explicit TcpConnection(TcpSocket socket, Endpoint remote)
      : socket_(std::move(socket)), local_(socket_.localEndpoint()),
        remote_(std::move(remote)) {}

  /**
   * @brief Retrieves the native socket handle.
   *
   * @return Platform-specific native socket descriptor.
   */
  Handle native_handle() const noexcept override {
    return socket_.native_handle();
  }

  /**
   * @brief Asynchronously reads data from the socket into the provided buffer.
   *
   * The operation suspends the coroutine until data becomes available or an
   * error occurs.
   *
   * @param buffer Destination buffer to store read bytes.
   * @return Number of bytes successfully read.
   */
  task<std::size_t> async_read(std::span<std::byte> buffer) override;

  /**
   * @brief Asynchronously writes data to the socket.
   *
   * The operation suspends the coroutine until all data is written or an
   * error occurs.
   *
   * @param buffer Source buffer containing bytes to send.
   */
  task<void> async_write(std::span<const std::byte> buffer) override;

  /**
   * @brief Asynchronously establishes a connection to the specified endpoint.
   *
   * @param ep Target endpoint to connect to.
   */
  task<void> async_connect(const Endpoint &ep);

  /**
   * @brief Retrieves the local endpoint of the connection.
   *
   * @return Local network endpoint.
   */
  Endpoint local_endpoint() const override { return local_; }

  /**
   * @brief Retrieves the remote endpoint of the connection.
   *
   * @return Remote network endpoint.
   */
  Endpoint remote_endpoint() const override { return remote_; }

  /**
   * @brief Closes the connection.
   */
  void close() override;

  /**
   * @brief Notification handler invoked when the socket becomes readable.
   */
  void notify_readable() override;

  /**
   * @brief Notification handler invoked when the socket becomes writable.
   */
  void notify_writable() override;

private:
  TcpSocket socket_;

  Endpoint local_;
  Endpoint remote_;

  std::coroutine_handle<> read_awaiting_;
  std::coroutine_handle<> write_awaiting_;

  std::span<std::byte> read_buffer_{};
  std::vector<std::byte> write_buffer_;

  bool closed_{false};
};

} // namespace net
