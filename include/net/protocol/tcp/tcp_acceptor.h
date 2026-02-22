#pragma once
#include "net/connection/iacceptor.h"
#include "net/connection/iconnection.h"
#include "net/core/endpoint.h"
#include "net/protocol/tcp/tcp_connection.h"
#include "tcp_socket.h"
#include <functional>
#include <memory>
#include <utility>

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
      : socket_(family) {}

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

  /**
   * @brief Asynchronously accepts an incoming connection.
   *
   * This function suspends the current coroutine until a new connection
   * is available, then returns the accepted connection wrapped in a
   * unique pointer to IConnection.
   *
   * @return A task representing the asynchronous accept operation.
   */
  task<std::unique_ptr<IConnection>> async_accept() override;

  /**
   * @brief Closes the acceptor socket.
   */
  void close() override { socket_.close(); }

  /**
   * @brief Notifies the acceptor that the socket has become readable.
   *
   * This is typically used to resume a coroutine waiting in async_accept().
   */
  void notify_readable();

  /**
   * @brief Binds the acceptor socket to the specified endpoint.
   *
   * @param ep Endpoint to bind to.
   */
  void bind(const Endpoint &ep) { socket_.bind(ep); }

  /**
   * @brief Starts listening for incoming connections.
   *
   * @param backlog Maximum length of the pending connection queue.
   */
  void listen(int backlog = SOMAXCONN) { socket_.listen(SOMAXCONN); }

private:
  TcpSocket socket_;
  std::coroutine_handle<> accept_awaiting_;
};

} // namespace net
