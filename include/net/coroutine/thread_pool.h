#pragma once
#include "trait.h"
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <coroutine>
#include <cstdint>
#include <deque>
#include <mutex>
#include <ranges>
#include <thread>
#include <vector>

namespace net {

/**
 * @brief A coroutine-aware thread pool scheduler.
 *
 * The `thread_pool` provides a mechanism to schedule coroutine execution
 * across a pool of worker threads. Coroutines can suspend using
 * `co_await pool.schedule()` and will later resume on one of the pool threads.
 *
 * The pool maintains a queue of coroutine handles which worker threads
 * consume and resume.
 */
class thread_pool {
public:
  /**
   * @brief Constructs a thread pool.
   *
   * Creates a number of worker threads that continuously process
   * scheduled coroutines.
   *
   * @param thread_count Number of worker threads to spawn.
   *        Defaults to `std::thread::hardware_concurrency()`.
   */
  explicit thread_pool(const std::uint_least32_t thread_count =
                           std::thread::hardware_concurrency())
      : stop_token_{false},
        thread_list_{std::views::iota(0) | std::views::take(thread_count) |
                     std::views::transform([this](auto) {
                       return std::thread{[this]() noexcept { thread_loop(); }};
                     }) |
                     std::ranges::to<std::vector<std::thread>>()} {}

  /**
   * @brief Destroys the thread pool.
   *
   * Signals all worker threads to stop and joins them.
   * Any remaining queued coroutines will not be executed.
   */
  ~thread_pool() {
    stop_token_.store(true, std::memory_order_release);

    condition_variable_.notify_all();

    std::ranges::for_each(thread_list_, [](auto &t) {
      if (t.joinable())
        t.join();
    });
  }

  /**
   * @brief Awaiter used to schedule a coroutine on the thread pool.
   *
   * When awaited, the current coroutine is suspended and its handle
   * is enqueued into the thread pool's work queue. A worker thread
   * later resumes the coroutine.
   */
  class schedule_awaiter {
  public:
    /**
     * @brief Always suspends the awaiting coroutine.
     *
     * @return false Always suspends.
     */
    [[nodiscard]]
    auto await_ready() const noexcept -> bool {
      return false;
    }

    /**
     * @brief Enqueues the coroutine for execution by a worker thread.
     *
     * @param coroutine Handle of the suspended coroutine.
     */
    auto await_suspend(std::coroutine_handle<> coroutine) const noexcept
        -> void {
      thread_pool_.enqueue(coroutine);
    }

    /**
     * @brief Called when the coroutine resumes.
     *
     * No result is returned.
     */
    auto await_resume() const noexcept -> void {}

  private:
    friend thread_pool;

    /**
     * @brief Constructs the awaiter bound to a thread pool.
     *
     * @param tp Reference to the thread pool scheduler.
     */
    explicit schedule_awaiter(thread_pool &tp) noexcept : thread_pool_{tp} {}

    /// Reference to the associated thread pool.
    thread_pool &thread_pool_;
  };

  /// Ensure the awaiter satisfies the awaitable concept.
  static_assert(awaitable<schedule_awaiter>);

  /**
   * @brief Returns an awaitable that schedules the coroutine on the pool.
   *
   * Usage example:
   *
   * @code
   * co_await pool.schedule();
   * @endcode
   *
   * The coroutine will resume on one of the thread pool's worker threads.
   */
  [[nodiscard]]
  auto schedule() noexcept -> schedule_awaiter {
    return schedule_awaiter{*this};
  }

  /**
   * @brief Returns the number of worker threads in the pool.
   *
   * @return Thread count.
   */
  [[nodiscard]]
  auto size() const noexcept -> std::uint_least32_t {
    return thread_list_.size();
  }

  /**
   * @brief Enqueues a coroutine handle for execution.
   *
   * The coroutine will later be resumed by a worker thread.
   *
   * @param coroutine Coroutine handle to schedule.
   */
  auto enqueue(std::coroutine_handle<> coroutine) -> void {
    {
      std::unique_lock lock(mutex_);
      coroutine_queue_.push_back(coroutine);
    }

    condition_variable_.notify_one();
  }

private:
  /**
   * @brief Worker thread execution loop.
   *
   * Continuously waits for new coroutine tasks and resumes them
   * until the thread pool is stopped.
   */
  auto thread_loop() noexcept -> void {
    while (!stop_token_.load(std::memory_order_acquire)) {

      std::coroutine_handle<> coroutine;

      {
        std::unique_lock lock(mutex_);

        condition_variable_.wait(lock, [this]() noexcept {
          return stop_token_.load(std::memory_order_acquire) ||
                 !coroutine_queue_.empty();
        });

        if (stop_token_.load(std::memory_order_acquire))
          return;

        coroutine = coroutine_queue_.front();
        coroutine_queue_.pop_front();
      }

      if (coroutine && !coroutine.done())
        coroutine.resume();
    }
  }

private:
  /// Mutex protecting the coroutine queue.
  std::mutex mutex_;

  /// Condition variable used to wake worker threads.
  std::condition_variable condition_variable_;

  /// Stop flag indicating pool shutdown.
  std::atomic<bool> stop_token_;

  /// Queue of pending coroutine handles.
  std::deque<std::coroutine_handle<>> coroutine_queue_;

  /// Worker threads executing scheduled coroutines.
  std::vector<std::thread> thread_list_;
};
} // namespace net
