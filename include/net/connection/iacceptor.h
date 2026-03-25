#pragma once
#include "net/connection/iconnection.h"
#include "net/core/endpoint.h"
#include "net/coroutine/task.h"
#include "net/detail/socket_handle.h"
#include <memory>
#include <vector>

namespace net {

/**
 * @brief Abstract interface representing a listening socket capable of
 * accepting connections.
 *
 * `IAcceptor` models a passive network endpoint (e.g., TCP listener)
 * that accepts incoming connections asynchronously.
 *
 * Implementations are responsible for:
 * - Binding to a local endpoint
 * - Listening for incoming connection requests
 * - Producing connection objects via `async_accept()`
 *
 * The interface is transport-agnostic and intended to support
 * different protocol implementations (e.g., TCP over IPv4/IPv6).
 *
 * This type is non-copyable and intended for polymorphic use.
 */
class IAcceptor : public std::enable_shared_from_this<IAcceptor> {
public:
  /// Native socket handle type.
  using Handle = detail::SocketDescriptorHandle::Handle;

  virtual ~IAcceptor() = default;

  /**
   * @brief Returns the underlying native socket handle.
   *
   * @return Platform-specific descriptor/handle.
   */
  virtual Handle handle() const noexcept = 0;

  /**
   * @brief Asynchronously accepts an incoming connection.
   *
   * Returns a coroutine task that completes when a new connection
   * has been established.
   *
   * @return A task producing a newly accepted connection.
   *
   * @throws std::system_error on fatal accept failure.
   *
   * @note The returned connection object owns the accepted socket.
   */
  virtual task<std::shared_ptr<IConnection>> async_accept() = 0;

  virtual task<std::vector<std::shared_ptr<IConnection>>>
  async_multi_accept() = 0;

  /**
   * @brief Returns the local endpoint the acceptor is bound to.
   *
   * @return The bound local endpoint.
   */
  virtual Endpoint local_endpoint() const = 0;

  /**
   * @brief Closes the acceptor socket.
   *
   * After calling this function:
   * - The underlying socket handle becomes invalid.
   * - Pending or future accept operations may fail.
   */
  virtual void close() = 0;

  virtual void bind(const Endpoint &ep) = 0;
  virtual void listen(int backlog) = 0;
  virtual std::shared_ptr<IConnection> try_accept() = 0;
};

} // namespace net
