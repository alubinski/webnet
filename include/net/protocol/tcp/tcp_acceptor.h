#pragma once
#include "net/connection/iacceptor.h"
#include "net/connection/iconnection.h"
#include "net/core/endpoint.h"
#include "net/coroutine/task.h"
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
  explicit TcpAcceptor(Reactor &reactor, TcpSocket::AddressFamily family =
                                             TcpSocket::AddressFamily::IPV4)
      : socket_(family), reactor_(reactor) {
    reactor_.register_fd(handle(), PollEvent::Read, nullptr);
  }

  /**
   * @brief Constructs a TCP acceptor by taking ownership of an existing socket.
   *
   * @param socket Existing TcpSocket instance to wrap.
   */
  explicit TcpAcceptor(TcpSocket socket, Reactor &reactor)
      : socket_(std::move(socket)), reactor_(reactor) {}

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
    Reactor &reactor;
    std::shared_ptr<TcpAcceptor> acceptor; // Your acceptor class
    std::shared_ptr<IConnection> result_conn;

    bool await_ready() {
      // try_accept() should call the non-blocking accept() syscall
      Endpoint peer;
      if (auto socket = acceptor->socket_.accept(peer)) {
        std::cerr << "[Accept] Immediate Success! FD: "
                  << socket->native_handle() << std::endl;
        result_conn =
            std::make_shared<TcpConnection>(std::move(*socket), peer, reactor);
        return true;
      }
      // result_conn = acceptor->socket_.accept(peer);

      // If we got a connection, DO NOT suspend!
      auto err = detail::last_socket_error();
      if (!detail::is_would_block(err)) {
        std::cerr << "[Accept] Fatal Error in await_ready: " << err
                  << std::endl;
      }
      return false;
    }

    void await_suspend(std::coroutine_handle<> handle) {
      // Register the listening socket to wake up when a new client connects
      // std::cerr << "[Accept] Suspending on Acceptor FD: "
      //           << acceptor->socket_.native_handle() << std::endl;
      reactor.register_fd(acceptor->socket_.native_handle(), PollEvent::Read,
                          handle);
    }

    std::shared_ptr<IConnection> await_resume() {
      if (result_conn)
        return result_conn;

      // We woke up from the reactor, try again!
      Endpoint peer;
      if (auto socket = acceptor->socket_.accept(peer)) {
        // std::cerr << "[Accept] Success after Resume! FD: "
        //           << socket->native_handle() << std::endl;
        return std::make_shared<TcpConnection>(std::move(*socket), peer,
                                               reactor);
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
    auto self = std::static_pointer_cast<TcpAcceptor>(shared_from_this());
    co_return co_await AcceptAwaiter{reactor_, self, nullptr};
  }

  struct AcceptBatchAwaiter {
    Reactor &reactor;
    // std::shared_ptr<TcpAcceptor> acceptor;
    TcpSocket &acceptor_sock;
    std::shared_ptr<IConnection> first_conn =
        nullptr; // Cache the first success
    const size_t max_batch{64};

    bool await_ready() {
      Endpoint peer;
      if (auto socket = acceptor_sock.accept(peer)) {
        int opt = 1;
        setsockopt(socket->native_handle(), IPPROTO_TCP, TCP_NODELAY, &opt,
                   sizeof(opt));
        first_conn =
            std::make_shared<TcpConnection>(std::move(*socket), peer, reactor);
        return true;
      }
      return false;
    } // Always suspend to wait for the reactor

    void await_suspend(std::coroutine_handle<> handle) {
      // if (!acceptor->is_registered_) {
      //   reactor.register_fd(acceptor->socket_.native_handle(),
      //   PollEvent::Read,
      //                       acceptor, handle);
      //   acceptor->is_registered_ = true;
      // } else {
      reactor.set_resume_handle(acceptor_sock.native_handle(), PollEvent::Read,
                                handle);
      // }
    }

    std::vector<std::shared_ptr<IConnection>> await_resume() {
      // std::cerr
      //     << "[AcceptBatch] await_resume() triggered. Draining backlog...\n"
      //     << std::flush;
      std::vector<std::shared_ptr<IConnection>> batch;
      if (first_conn) {
        batch.push_back(std::move(first_conn));
      }

      // Drain the backlog completely INSIDE the awaiter
      while (batch.size() < max_batch) {
        Endpoint peer;
        if (auto socket = acceptor_sock.accept(peer)) {
          int opt = 1;
          setsockopt(socket->native_handle(), IPPROTO_TCP, TCP_NODELAY, &opt,
                     sizeof(opt));
          // std::cerr << "[AcceptBatch] Accepted FD: " <<
          // socket->native_handle()
          //           << "\n"
          //           << std::flush;
          batch.push_back(std::make_shared<TcpConnection>(std::move(*socket),
                                                          peer, reactor));
        } else {
          int err = errno;
          if (err != EAGAIN && err != EWOULDBLOCK) {
            // std::cerr << "[AcceptBatch] Drain stopped by error: "
            //           << strerror(err) << "\n"
            //           << std::flush;
          }
          break; // EAGAIN hit, backlog is empty
        }
      }
      return batch;
    }
  };

  task<std::vector<std::shared_ptr<IConnection>>>
  async_multi_accept() override {
    auto self = std::static_pointer_cast<TcpAcceptor>(shared_from_this());
    co_return co_await AcceptBatchAwaiter{reactor_, socket_};
  }
  /**
   * @brief Closes the acceptor socket.
   */
  void close() override {
    reactor_.stop();
    socket_.close();
  }

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

  // Inside TcpAcceptor.cpp
  std::shared_ptr<IConnection> try_accept() override {
    Endpoint peer;
    auto socket = socket_.accept(peer); // socket_.accept should be non-blocking
    if (socket) {
      return std::make_shared<TcpConnection>(std::move(*socket), peer,
                                             reactor_);
    }
    return nullptr; // Returns null if EAGAIN/EWOULDBLOCK
  }

private:
  TcpSocket socket_;

  Reactor &reactor_;

  bool is_registered_{false};
};

} // namespace net
