#pragma once

#include "net/connection/iconnection.h"
#include "net/coroutine/thread_pool.h"
#include "net/poll/ipoll.h"
#include <coroutine>
#include <iostream>
#include <memory>
#include <net/poll/poll_factory.h>
#include <unordered_map>

namespace net {

struct EventContext {
  std::coroutine_handle<> handle;
  // std::weak_ptr<void> resource;
  PollEvent event_type;
};

/**
 * @brief Asynchronous I/O reactor implementing the Reactor Pattern.
 *
 * The `Reactor` monitors file descriptors for I/O readiness using a
 * platform-specific polling backend (epoll, kqueue, poll, etc.).
 *
 * When a file descriptor becomes ready, the corresponding coroutine
 * continuation is scheduled on the provided thread pool.
 *
 * This decouples:
 * - **I/O event detection** (handled by the reactor thread)
 * - **coroutine execution** (handled by the thread pool)
 *
 * The design allows scalable asynchronous I/O while keeping the event
 * loop lightweight.
 */
class Reactor {
public:
  /**
   * @brief Constructs a Reactor instance.
   *
   * @param pool Reference to the thread pool used for coroutine execution.
   * @param poller Platform-specific polling implementation.
   */
  Reactor(thread_pool &pool, std::unique_ptr<detail::IPoll> poller)
      : pool_{pool}, poller_{std::move(poller)} {}

  /**
   * @brief Starts the reactor event loop.
   *
   * Launches a dedicated thread that continuously waits for I/O events
   * and schedules corresponding coroutine continuations.
   */
  void start() {
    running_.store(true, std::memory_order_release);

    reactor_thread_ = std::thread([this] { loop(); });
  }

  /**
   * @brief Stops the reactor event loop.
   *
   * Signals the loop to terminate and joins the reactor thread.
   */
  void stop() {
    running_.store(false, std::memory_order_release);

    if (reactor_thread_.joinable())
      reactor_thread_.join();
  }

  /**
   * @brief Registers a file descriptor for asynchronous polling.
   *
   * Associates a coroutine continuation with the file descriptor.
   * When the descriptor becomes ready, the coroutine will be scheduled
   * on the thread pool.
   *
   * @param fd File descriptor to monitor.
   * @param events Polling events of interest (read, write, etc.).
   * @param continuation Coroutine handle to resume when the event occurs.
   *
   * @note The underlying poll implementation should use **edge-triggered**
   * and **one-shot** semantics (e.g. `EPOLLET | EPOLLONESHOT`).
   */

  void register_fd(fd_t fd, PollEvent events,
                   std::coroutine_handle<> continuation) {

    EventContext ctx{.handle = continuation, .event_type = events};
    {
      std::scoped_lock lock(mutex_);
      continuations_[fd] = ctx;
    }

    poller_->add(fd, events);
  }
  // void register_fd(fd_t fd, PollEvent events, std::shared_ptr<void> resource,
  //                  std::coroutine_handle<> continuation) {
  //
  //   EventContext ctx{
  //       .handle = continuation, .resource = resource, .event_type = events};
  //   {
  //     std::scoped_lock lock(mutex_);
  //     continuations_[fd] = ctx;
  //   }
  //
  //   poller_->add(fd, events);
  // }

  void set_resume_handle(fd_t fd, PollEvent events,
                         std::coroutine_handle<> continuation) {

    EventContext ctx{.handle = continuation, .event_type = events};
    {
      std::scoped_lock lock(mutex_);
      continuations_[fd] = ctx;
    }
  }

  /**
   * @brief Unregisters a file descriptor from polling.
   *
   * Removes the associated coroutine continuation and stops monitoring
   * the descriptor.
   *
   * @param fd File descriptor to remove.
   */
  void unregister_fd(fd_t fd) {
    std::scoped_lock lock(mutex_);
    continuations_.erase(fd);
    poller_->remove(fd);
  }

private:
  /**
   * @brief Main reactor event loop.
   *
   * Continuously waits for I/O events using the polling backend.
   * When an event occurs, the corresponding coroutine continuation
   * is retrieved and scheduled on the thread pool.
   */
  void loop() {
    // 1. Pre-allocate a vector for handles to avoid enqueuing inside the lock
    std::vector<std::coroutine_handle<>> ready_handles;
    ready_handles.reserve(64);

    while (running_.load(std::memory_order_acquire)) {
      auto results = poller_->wait(100); // 100ms is fine

      {
        // 2. Lock ONCE for the whole batch
        std::scoped_lock lock(mutex_);

        for (auto &result : results) {
          // 3. Use a vector/array for O(1) direct access (no hashing!)
          if (result.fd < continuations_.size()) {
            auto h = continuations_[result.fd];
            if (h.handle && !h.handle.done()) {
              ready_handles.push_back(h.handle);
              continuations_[result.fd].handle = nullptr; // Clear it (one-shot)
            }
          }
        }
      }

      // 4. Batch-enqueue to the pool (requires a new pool.enqueue_batch method)
      if (!ready_handles.empty()) {
        pool_.enqueue_batch(ready_handles);
        ready_handles.clear();
      }
    }
  }
  // void loop() {
  //   while (running_.load(std::memory_order_acquire)) {
  //     // Use a short timeout or signal-based wake up
  //     auto results = poller_->wait(100);
  //
  //     // if (results.empty()) {
  //     //   std::cerr << "[Reactor] Heartbeat: Still waiting for events..."
  //     //             << std::endl;
  //     // }
  //
  //     // --- Inside Reactor::loop() ---
  //     for (auto &result : results) {
  //       // std::coroutine_handle<> handle;
  //
  //       {
  //         std::scoped_lock lock(mutex_);
  //         auto it = continuations_.find(result.fd);
  //         if (it != continuations_.end()) {
  //           auto &ctx = it->second;
  //
  //           // if (auto connection = ctx.resource.lock()) {
  //           auto h = ctx.handle;
  //           continuations_.erase(it);
  //           if (h && !h.done()) {
  //             pool_.enqueue(h, true);
  //           }
  //           // h.resume();
  //           // }
  //           // handle = it->second;
  //           // Remove from map to satisfy one-shot requirements
  //           // and prevent double-resumption.
  //           // continuations_.erase(it);
  //         }
  //       }
  //
  //       // Pass the handle directly to the pool as your signature requires.
  //       // The pool will be responsible for calling handle.resume().
  //       // if (handle && !handle.done()) {
  //       //   pool_.enqueue(handle);
  //       // }
  //     }
  //   }
  // }

  /// Thread pool used for executing resumed coroutines.
  thread_pool &pool_;

  /// Platform-specific poll implementation.
  std::unique_ptr<detail::IPoll> poller_;

  /// Reactor running state.
  std::atomic<bool> running_{false};

  /// Dedicated thread executing the event loop.
  std::thread reactor_thread_;

  /// Mutex protecting the continuation map.
  std::mutex mutex_;

  /**
   * @brief Mapping between file descriptors and coroutine continuations.
   *
   * Each descriptor corresponds to a suspended coroutine waiting
   * for an I/O event.
   */
  std::unordered_map<fd_t, EventContext> continuations_;
};

} // namespace net
