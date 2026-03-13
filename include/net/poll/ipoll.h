#pragma once
#include "net/detail/socket_handle.h"
#include <cstdint>
#include <vector>

namespace net {

using fd_t = detail::SocketDescriptorHandle::Handle;

/**
 * @brief Enumeration of pollable I/O events.
 *
 * These flags describe which I/O readiness conditions should be
 * monitored for a given file descriptor.
 */
enum class PollEvent : uint8_t {
  None = 0,
  Read = 1 << 0,
  Write = 1 << 1,
  Error = 1 << 2
};

/**
 * @brief Bitmask representation of poll events.
 *
 * Used internally by polling implementations when combining
 * multiple @ref PollEvent flags.
 */
using PollEventMask = uint8_t;

/**
 * @brief Bitmask representation of poll events.
 *
 * Used internally by polling implementations when combining
 * multiple @ref PollEvent flags.
 */
inline PollEvent operator|(PollEvent a, PollEvent b) {
  return static_cast<PollEvent>(static_cast<uint8_t>(a) |
                                static_cast<uint8_t>(b));
}

/**
 * @brief Computes the intersection of two poll events.
 */
inline PollEvent operator&(PollEvent a, PollEvent b) {
  return static_cast<PollEvent>(static_cast<uint8_t>(a) &
                                static_cast<uint8_t>(b));
}

/**
 * @brief Result returned from a polling operation.
 *
 * Represents a file descriptor that has triggered one or more
 * readiness events.
 */
struct PollResult {
  detail::SocketDescriptorHandle::Handle fd;
  PollEvent events;
};

namespace detail {
/**
 *
 * @brief Abstract interface for platform-specific polling backends.
 *
 * `IPoll` defines the common interface used by the @ref net::Reactor
 * to monitor file descriptors for I/O readiness.
 *
 * Implementations typically wrap system APIs such as:
 *
 * - `epoll` (Linux)
 * - `kqueue` (BSD/macOS)
 * - `poll` (portable fallback)
 *
 * The goal of this abstraction is to isolate OS-specific code from
 * the higher-level reactor logic.
 */
class IPoll {
public:
  virtual ~IPoll() = default;

  /**
   * @brief Adds a file descriptor to the polling set.
   *
   * @param fd File descriptor to monitor.
   * @param events Events to watch for.
   */
  virtual void add(SocketDescriptorHandle::Handle fd, PollEvent events) = 0;

  /**
   * @brief Modifies the events associated with a file descriptor.
   *
   * @param fd File descriptor to update.
   * @param events New set of events to monitor.
   */
  virtual void modify(SocketDescriptorHandle::Handle fd, PollEvent events) = 0;

  /**
   * @brief Removes a file descriptor from the polling set.
   *
   * After removal, no further events will be generated for the descriptor.
   *
   * @param fd File descriptor to remove.
   */
  virtual void remove(SocketDescriptorHandle::Handle fd) = 0;

  /**
   * @brief Cancels any pending operations for the descriptor.
   *
   * This function can be used to wake up waiting coroutines or
   * abort pending I/O operations associated with the descriptor.
   *
   * @param fd File descriptor whose events should be cancelled.
   */
  virtual void cancel(SocketDescriptorHandle::Handle fd) = 0;

  /**
   * @brief Waits for I/O events.
   *
   * Blocks until events occur or the timeout expires.
   *
   * @param timeout_ms Maximum wait time in milliseconds.
   *
   * @return Vector of poll results describing triggered events.
   */
  virtual std::vector<PollResult> wait(int timeout_ms) = 0;
};

} // namespace detail
} // namespace net
