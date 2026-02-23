#pragma once
#include "net/core/endpoint.h"
#include "net/detail/socket_handle.h"
#include "net/detail/task.h"
#include <cstddef>
#include <span>

namespace net {

/**
 * @brief Abstract interface representing an established network connection.
 *
 * `IConnection` models a full-duplex connected socket (e.g., TCP).
 * It provides asynchronous read and write operations integrated
 * with the library's coroutine-based execution model.
 *
 * Implementations are responsible for:
 * - Managing the lifetime of the underlying socket
 * - Performing non-blocking I/O
 * - Resuming suspended coroutines when readiness is signaled
 *
 * This interface is transport-agnostic and may be implemented
 * for TCP, UTP, or other stream-oriented protocols.
 *
 * The type is intended for polymorphic use and is non-copyable.
 */
class IConnection {
public:
  virtual ~IConnection() = default;

  /**
   * @brief Returns the native socket handle.
   *
   * @return Platform-specific descriptor/handle.
   */
  virtual detail::SocketDescriptorHandle::Handle
  native_handle() const noexcept = 0;

  /**
   * @brief Asynchronously reads data into the provided buffer.
   *
   * The operation completes when:
   * - At least one byte is read
   * - The connection is closed by the peer
   * - A fatal error occurs
   *
   * @param buffer Destination buffer for received data.
   * @return A task resolving to the number of bytes read.
   *
   * @throws std::system_error on fatal I/O failure.
   *
   * @note A return value of 0 typically indicates EOF.
   */
  virtual task<std::size_t> async_read(std::span<std::byte> buffer) = 0;

  /**
   * @brief Asynchronously writes data from the provided buffer.
   *
   * The operation completes once all bytes have been written
   * or a fatal error occurs.
   *
   * @param buffer Source buffer containing data to send.
   * @return A task that completes when the write operation finishes.
   *
   * @throws std::system_error on fatal I/O failure.
   */
  virtual task<void> async_write(std::span<const std::byte> buffer) = 0;

  /**
   * @brief Returns the local endpoint of the connection.
   *
   * @return The local address and port.
   */
  virtual Endpoint local_endpoint() const = 0;

  /**
   * @brief Returns the local endpoint of the connection.
   *
   * @return The local address and port.
   */
  virtual Endpoint remote_endpoint() const = 0;

  /**
   * @brief Notifies the connection that the socket is readable.
   *
   * Typically called by an event loop or I/O multiplexer
   * when the underlying descriptor becomes readable.
   *
   * Suspended read operations may resume as a result.
   */
  virtual void notify_readable() = 0;

  /**
   * @brief Notifies the connection that the socket is writable.
   *
   * Typically called by an event loop or I/O multiplexer
   * when the underlying descriptor becomes writable.
   *
   * Suspended write operations may resume as a result.
   */
  virtual void notify_writable() = 0;

  /**
   * @brief Notifies the connection that the socket is writable.
   *
   * Typically called by an event loop or I/O multiplexer
   * when the underlying descriptor becomes writable.
   *
   * Suspended write operations may resume as a result.
   */
  virtual void close() = 0;
};

} // namespace net
