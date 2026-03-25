#pragma once
#include "net/connection/iconnection.h"
#include "net/core/endpoint.h"
#include "net/coroutine/reactor.h"
#include "net/detail/socket_flags.h"
#include "net/detail/socket_handle.h"
#include "tcp_socket.h"
#include <algorithm>
#include <coroutine>
#include <memory>
#include <span>
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
  explicit TcpConnection(TcpSocket socket, Endpoint remote, Reactor &reactor)
      : socket_(std::move(socket)), local_(socket_.localEndpoint()),
        remote_(std::move(remote)), reactor_(reactor) {
    // This is the "Magic Bullet" for EOF issues in high-speed servers
    struct linger sl;
    sl.l_onoff = 1;  // Enable lingering
    sl.l_linger = 0; // Set to 0 to force an RST, or 1+ to wait for FIN
    // Actually, for your case, try:
    sl.l_linger = 2; // Give it 2 seconds of "kernel life" after close()
    setsockopt(socket_.native_handle(), SOL_SOCKET, SO_LINGER, &sl, sizeof(sl));
  }

  ~TcpConnection() {
    if (socket_.is_valid()) {
      reactor_.unregister_fd(native_handle());
      // socket_.shutdown(detail::SocketFlags::ShutdownType::Sending);
      // socket_.close();
    }
  }

  /**
   * @brief Retrieves the native socket handle.
   *
   * @return Platform-specific native socket descriptor.
   */
  Handle native_handle() const noexcept override {
    return socket_.native_handle();
  }

  struct RecvAwaiter {
    Reactor &reactor;
    TcpSocket &conn_sock;
    // std::shared_ptr<TcpConnection> conn;
    std::span<std::byte> buffer;

    std::size_t result = 0;

    bool await_ready() {
      int received = conn_sock.receive(buffer);
      if (received >= 0) {
        result = static_cast<std::size_t>(received);
        return true; // Don't suspend, go straight to await_resume
      }

      auto err = detail::last_socket_error();
      if (detail::is_would_block(err)) {
        return false; // Suspend and call await_suspend
      }

      throw std::runtime_error("recv failed in await_ready");
    }

    // 2. We couldn't read immediately, so we register with the Reactor.
    void await_suspend(std::coroutine_handle<> handle) {
      reactor.register_fd(conn_sock.native_handle(), PollEvent::Read, handle);
    }

    // 3. When the Reactor resumes us, the result is returned to the caller.
    std::size_t await_resume() {
      // If await_ready returned true, 'result' is already set.
      // If we suspended, we need to try reading again now that we are "Ready".
      if (result > 0 || buffer.empty())
        return result;

      int received = conn_sock.receive(buffer);
      if (received >= 0) {
        return static_cast<std::size_t>(received);
      }

      // If we get here and it's still EAGAIN, something is wrong with the
      // Reactor's signaling.
      throw std::runtime_error("recv failed in await_resume");
    }
  };

  /**
   * @brief Asynchronously reads data from the socket into the provided buffer.
   *
   * The operation suspends the coroutine until data becomes available or an
   * error occurs.
   *
   * @param buffer Destination buffer to store read bytes.
   * @return Number of bytes successfully read.
   */
  task<std::size_t> async_read(std::span<std::byte> buffer) override {
    // auto self = std::static_pointer_cast<TcpConnection>(shared_from_this());
    co_return co_await RecvAwaiter{reactor_, this->socket_, buffer};
  }

  struct SendAwaiter {
    Reactor &reactor;
    // std::shared_ptr<TcpConnection> conn;
    TcpSocket &conn_sock;
    std::span<const std::byte> buffer;

    std::size_t bytes_sent = 0;

    // 1. Try to send immediately
    bool await_ready() {
      if (buffer.empty())
        return true;

      int n = conn_sock.send(buffer);
      if (n > 0) {
        bytes_sent = static_cast<std::size_t>(n);
        // If we sent everything, don't suspend!
        return bytes_sent == buffer.size();
      }

      auto err = detail::last_socket_error();
      if (detail::is_would_block(err))
        return false; // Suspend

      throw std::runtime_error("send failed in await_ready");
    }

    void await_suspend(std::coroutine_handle<> handle) {
      reactor.register_fd(conn_sock.native_handle(), PollEvent::Write, handle);
    }

    std::size_t await_resume() {
      // If we already sent everything in await_ready, return that.
      if (bytes_sent > 0 || buffer.empty())
        return bytes_sent;

      // Otherwise, we were resumed because the socket is now writable.
      int n = conn_sock.send(buffer);
      if (n >= 0)
        return static_cast<std::size_t>(n);

      throw std::runtime_error("send failed in await_resume");
    }
  };

  /**
   * @brief Asynchronously writes data to the socket.
   *
   * The operation suspends the coroutine until all data is written or an
   * error occurs.
   *
   * @param buffer Source buffer containing bytes to send.
   */
  task<void> async_write(std::span<const std::byte> buffer) override {
    auto self = std::static_pointer_cast<TcpConnection>(shared_from_this());
    auto remaining = buffer;

    while (!remaining.empty()) {
      // The awaiter handles the suspension if the socket buffer is full
      std::size_t n = co_await SendAwaiter{reactor_, this->socket_, remaining};

      if (n == 0)
        throw std::runtime_error("connection closed during write");

      remaining = remaining.subspan(n);
    }
  }

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
  // void notify_readable() override;

  /**
   * @brief Notification handler invoked when the socket becomes writable.
   */
  // void notify_writable() override;

  void shutdown(detail::SocketFlags::ShutdownType how) override {
    socket_.shutdown(how);
  }

private:
  TcpSocket socket_;

  Endpoint local_;
  Endpoint remote_;

  Reactor &reactor_;

  std::span<std::byte> read_buffer_{};
  std::vector<std::byte> write_buffer_;

  bool closed_{false};
};

} // namespace net
