#pragma once
#include "net/connection/iconnection.h"
#include "net/core/endpoint.h"
#include "net/detail/socket_handle.h"
#include "net/detail/task.h"
#include <memory>

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
class IAcceptor {
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
  virtual task<std::unique_ptr<IConnection>> async_accept() = 0;

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
};

} // namespace net
