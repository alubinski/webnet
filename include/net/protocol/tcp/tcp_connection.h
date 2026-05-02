#pragma once
#include "net/connection/iconnection.h"
#include "net/core/endpoint.h"
#include "net/detail/socket_handle.h"
#include "net/poll/epoll_context.h"
#include "tcp_socket.h"
#include <algorithm>
#include <coroutine>
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

  class read_guard {
  public:
    explicit read_guard(TcpSocket &socket, std::span<std::byte> &buffer,
                        std::size_t len) noexcept
        : socket_(socket), buffer_(buffer), len_(len) {}

    ~read_guard() noexcept {
      epoll_context::get_instance().unwatch(socket_.native_handle());
    }

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> h) noexcept {
      // Watch for Input and use Edge-Triggered + OneShot
      epoll_context::get_instance().watch(socket_.native_handle(),
                                          EPOLLIN | EPOLLET, h);
    }

    // Returns number of bytes read, 0 for disconnect, or -1 for error
    [[nodiscard]] ssize_t await_resume() const noexcept {
      return socket_.receive(buffer_);
    }

  private:
    TcpSocket &socket_;
    std::span<std::byte> &buffer_;
    std::size_t len_;
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
    co_return co_await read_guard{socket_, buffer, 0};
  }

  class write_guard {
  public:
    // We take a const void* because we are only reading from this buffer to
    // send it
    explicit write_guard(TcpSocket &socket, std::span<const std::byte> &buffer,
                         std::size_t len) noexcept
        : socket_(socket), buffer_(buffer), len_(len) {}

    ~write_guard() noexcept {
      // Stop watching the socket when the guard goes out of scope
      epoll_context::get_instance().unwatch(socket_.native_handle());
    }

    // Always suspend to ensure we are synchronized with the epoll loop
    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> h) noexcept {
      // Watch for Output (readiness to write) + Edge-Triggered
      epoll_context::get_instance().watch(socket_.native_handle(),
                                          EPOLLOUT | EPOLLET, h);
    }

    // Returns number of bytes sent, or -1 for error
    [[nodiscard]] ssize_t await_resume() const noexcept {
      // MSG_NOSIGNAL is critical for servers: it prevents SIGPIPE
      // if the client closed the connection before we finished writing.

      return socket_.send(buffer_);
    }

  private:
    TcpSocket &socket_;
    std::span<const std::byte> buffer_;
    std::size_t len_;
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
    auto remaining = buffer;

    while (!remaining.empty()) {
      // The awaiter handles the suspension if the socket buffer is full
      std::size_t n = co_await write_guard{this->socket_, remaining, 0};

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

private:
  TcpSocket socket_;

  Endpoint local_;
  Endpoint remote_;

  std::span<std::byte> read_buffer_{};
  std::vector<std::byte> write_buffer_;

  bool closed_{false};
};

} // namespace net
