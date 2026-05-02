#pragma once
#include "net/connection/iacceptor.h"
#include "net/connection/iconnection.h"
#include "net/core/endpoint.h"
#include "net/coroutine/task.h"
#include "net/poll/epoll_context.h"
#include "net/poll/ipoll.h"
#include "net/protocol/tcp/tcp_connection.h"
#include "tcp_socket.h"
#include <cstring>
#include <functional>
#include <memory>
#include <netinet/tcp.h>
#include <utility>
#include <vector>

namespace net {

/**
 * @brief TCP implementation of the IAcceptor interface.
 *
 * TcpAcceptor is responsible for listening on a TCP endpoint and asynchronously
 * accepting incoming connections. It manages an underlying TcpSocket and
 * supports coroutine-based acceptance via async_accept().
 */
class TcpAcceptor : public IAcceptor {
public:
  /**
   * @brief Constructs a TCP acceptor with the specified address family.
   *
   * @param family Address family to use for the underlying socket.
   */
  explicit TcpAcceptor(
      TcpSocket::AddressFamily family = TcpSocket::AddressFamily::IPV4)
      : socket_(family) {

    epoll_context::get_instance().watch(socket_.native_handle(),
                                        static_cast<uint32_t>(PollEvent::Read),
                                        nullptr);
  }

  /**
   * @brief Constructs a TCP acceptor by taking ownership of an existing socket.
   *
   * @param socket Existing TcpSocket instance to wrap.
   */
  explicit TcpAcceptor(TcpSocket socket) : socket_(std::move(socket)) {}

  /**
   * @brief Retrieves the native socket handle.
   *
   * @return The platform-specific native socket handle.
   */
  Handle handle() const noexcept override { return socket_.native_handle(); }

  /**
   * @brief Retrieves the local endpoint bound to the acceptor socket.
   *
   * @return Local network endpoint.
   */
  Endpoint local_endpoint() const override { return socket_.localEndpoint(); }

  struct AcceptAwaiter {
    // std::shared_ptr<TcpAcceptor> acceptor; // Your acceptor class
    TcpSocket &acceptor;
    std::shared_ptr<IConnection> result_conn;

    bool await_ready() {
      // try_accept() should call the non-blocking accept() syscall
      Endpoint peer;
      if (auto socket = acceptor.accept(peer)) {
        // std::cerr << "[Accept] Immediate Success! FD: "
        //           << socket->native_handle() << std::endl;
        result_conn = std::make_shared<TcpConnection>(std::move(*socket), peer);
        return true;
      }
      return false;
    }

    void await_suspend(std::coroutine_handle<> handle) {
      // Register the listening socket to wake up when a new client connects
      // std::cerr << "[Accept] Suspending on Acceptor FD: "
      //           << acceptor->socket_.native_handle() << std::endl;
      // reactor.register_fd(acceptor->socket_.native_handle(), PollEvent::Read,
      //                     handle);
      epoll_context::get_instance().watch(
          acceptor.native_handle(), static_cast<uint32_t>(PollEvent::Read),
          handle);
    }

    std::shared_ptr<IConnection> await_resume() {
      if (result_conn)
        return result_conn;

      // We woke up from the reactor, try again!
      Endpoint peer;
      if (auto socket = acceptor.accept(peer)) {
        // std::cerr << "[Accept] Success after Resume! FD: "
        //           << socket->native_handle() << std::endl;
        return std::make_shared<TcpConnection>(std::move(*socket), peer);
      }

      auto err = detail::last_socket_error();
      // std::cerr << "[Accept] Resume triggered but accept failed! Err: " <<
      // err
      //           << std::endl;
      //
      return nullptr;

      // throw std::runtime_error("Accept failed after reactor wake-up");
    }
  };

  /**
   * @brief Asynchronously accepts an incoming connection.
   *
   * This function suspends the current coroutine until a new connection
   * is available, then returns the accepted connection wrapped in a
   * unique pointer to IConnection.
   *
   * @return A task representing the asynchronous accept operation.
   */
  task<std::shared_ptr<IConnection>> async_accept() override {
    // auto self = std::static_pointer_cast<TcpAcceptor>(shared_from_this());
    co_return co_await AcceptAwaiter{socket_, nullptr};
  }

  /**
   * @brief Closes the acceptor socket.
   */
  void close() override { socket_.close(); }

  /**
   * @brief Binds the acceptor socket to the specified endpoint.
   *
   * @param ep Endpoint to bind to.
   */
  void bind(const Endpoint &ep) override { socket_.bind(ep); }

  /**
   * @brief Starts listening for incoming connections.
   *
   * @param backlog Maximum length of the pending connection queue.
   */
  void listen(int backlog = SOMAXCONN) override { socket_.listen(SOMAXCONN); }

private:
  TcpSocket socket_;
};

} // namespace net
