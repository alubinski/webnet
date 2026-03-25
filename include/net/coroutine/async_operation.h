#pragma once
#include "net/connection/iconnection.h"
#include "net/poll/ipoll.h"
#include "reactor.h"

#include <atomic>
#include <coroutine>
#include <memory>
#include <mutex>

namespace net {

/**
 * @brief Awaitable representing an asynchronous I/O operation.
 *
 * `async_operation` suspends the awaiting coroutine until a specified
 * file descriptor becomes ready for the requested poll events
 * (e.g., readable or writable).
 *
 * The operation registers the coroutine continuation with the
 * @ref Reactor. Once the reactor detects the event, the coroutine
 * will be resumed on the associated thread pool.
 *
 * Typical usage:
 *
 * @code
 * co_await async_operation(reactor, socket_fd, PollEvent::Read);
 * @endcode
 *
 * This design enables non-blocking I/O while keeping coroutine code
 * straightforward and sequential in appearance.
 */
class async_operation {
public:
  /**
   * @brief Constructs an asynchronous operation awaitable.
   *
   * @param r Reference to the reactor responsible for monitoring the file
   * descriptor.
   * @param fd File descriptor to monitor.
   * @param events Poll events to wait for (read, write, etc.).
   */
  async_operation(Reactor &r, std::shared_ptr<IConnection> conn,
                  PollEvent events)
      : reactor_{r}, conn_{conn}, events_{events} {}

  /**
   * @brief Determines if the coroutine should suspend.
   *
   * Always returns false, meaning the coroutine will suspend and
   * wait for the reactor to signal readiness.
   *
   * @return false Always suspends.
   */
  bool await_ready() const noexcept { return false; }

  /**
   * @brief Suspends the coroutine and registers it with the reactor.
   *
   * The reactor will monitor the file descriptor and schedule
   * the coroutine for execution when the event occurs.
   *
   * @param h Handle of the suspended coroutine.
   */
  void await_suspend(std::coroutine_handle<> h) {
    reactor_.register_fd(conn_->native_handle(), events_, h);
  }

  /**
   * @brief Called when the coroutine resumes.
   *
   * This function does not return a value; it simply resumes execution
   * after the awaited event becomes ready.
   */
  void await_resume() const noexcept {}

private:
  /// Reference to the reactor handling I/O readiness events.
  Reactor &reactor_;

  /// File descriptor associated with the operation.
  // fd_t fd_;

  std::shared_ptr<IConnection> conn_;

  /// Poll events the coroutine is waiting for.
  PollEvent events_;
};
} // namespace net
