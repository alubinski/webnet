#pragma once
#include "net/coroutine/task.h"
#include "trait.h"
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <coroutine>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <ranges>
#include <thread>
#include <vector>

namespace net {

struct WorkerQueue {
  std::mutex mutex;
  std::condition_variable cv;
  std::deque<std::coroutine_handle<>> tasks;
  // ...
};

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
  // explicit thread_pool(const std::uint_least32_t thread_count =
  //                          std::thread::hardware_concurrency())
  //     : stop_token_{false}, thread_count_(thread_count),
  //       thread_list_{std::views::iota(0) | std::views::take(thread_count) |
  //                    std::views::transform([this](auto) {
  //                      return std::thread{[this]() noexcept { thread_loop();
  //                      }};
  //                    }) |
  //                    std::ranges::to<std::vector<std::thread>>()} {}
  explicit thread_pool(const std::uint_least32_t thread_count =
                           std::thread::hardware_concurrency())
      : stop_token_{false}, thread_count_{thread_count},
        // 1. Create one shard per thread first
        shards_{std::views::iota(0u, thread_count) |
                std::views::transform(
                    [](auto) { return std::make_unique<WorkerQueue>(); }) |
                std::ranges::to<std::vector>()},
        // 2. Launch threads, passing their specific index
        thread_list_{std::views::iota(0u, thread_count) |
                     std::views::transform([this](std::uint_least32_t i) {
                       return std::thread{
                           [this, i]() noexcept { thread_loop(i); }};
                     }) |
                     std::ranges::to<std::vector<std::thread>>()} {}
  /**
   * @brief Destroys the thread pool.
   *
   * Signals all worker threads to stop and joins them.
   * Any remaining queued coroutines will not be executed.
   */
  // ~thread_pool() {
  //   stop_token_.store(true, std::memory_order_release);
  //
  //   condition_variable_.notify_all();
  //
  //   std::ranges::for_each(thread_list_, [](auto &t) {
  //     if (t.joinable())
  //       t.join();
  //   });
  // }
  ~thread_pool() {
    // 1. Signal stop
    stop_token_.store(true, std::memory_order_release);

    // 2. Wake up every single shard's thread
    for (auto &shard : shards_) {
      shard->cv.notify_all();
    }

    // 3. Join as usual
    std::ranges::for_each(thread_list_, [](auto &t) {
      if (t.joinable())
        t.join();
    });
  }

  void detach(task<void> &&t) {
    auto handle = t.take_handle(); // Function to get handle and set task's
                                   // handle to nullptr
    if (handle) {
      handle.promise().detached = true;
      this->enqueue(handle);
    }
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

  void enqueue_batch(std::span<std::coroutine_handle<>> handles) {
    if (handles.empty())
      return;

    // 1. Group handles by the shard they will go to
    // This prevents us from locking the same shard multiple times in one batch
    std::vector<std::vector<std::coroutine_handle<>>> distributions(
        thread_count_);

    for (auto h : handles) {
      size_t target_shard =
          next_.fetch_add(1, std::memory_order_relaxed) % thread_count_;
      distributions[target_shard].push_back(h);
    }

    // 2. Push to each shard and notify
    for (size_t i = 0; i < thread_count_; ++i) {
      if (distributions[i].empty())
        continue;

      auto &shard = shards_[i];
      {
        std::lock_guard lock(shard->mutex);
        for (auto h : distributions[i]) {
          shard->tasks.push_back(h);
        }
      }

      // If we added multiple tasks, notify_all might be better to wake
      // up any idle workers assigned to this shard.
      if (distributions[i].size() > 1) {
        shard->cv.notify_all();
      } else {
        shard->cv.notify_one();
      }
    }
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
  auto enqueue(std::coroutine_handle<> coroutine, bool priority = false,
               int preferred_queue = -1) -> void {

    int index = (preferred_queue != -1) ? (preferred_queue % thread_count_)
                                        : (next_++ % thread_count_);

    auto &q = shards_[index];
    {
      std::lock_guard lock(q->mutex);
      q->tasks.push_back(coroutine);
    }
    q->cv.notify_one();
    // {
    //
    //   std::unique_lock lock(mutex_);
    //   if (priority) {
    //     coroutine_queue_.push_front(coroutine);
    //   } else {
    //     coroutine_queue_.push_back(coroutine);
    //   }
    // }
    //
    // condition_variable_.notify_one();
  }

private:
  /**
   * @brief Worker thread execution loop.
   *
   * Continuously waits for new coroutine tasks and resumes them
   * until the thread pool is stopped.
   */
  auto thread_loop(size_t index) noexcept -> void {
    // Get a reference to THIS thread's specific shard
    auto &shard = shards_[index];

    while (!stop_token_.load(std::memory_order_acquire)) {
      std::coroutine_handle<> coroutine;

      {
        // 🔒 Lock ONLY this thread's private mutex
        std::unique_lock lock(shard->mutex);

        // 💤 Wait ONLY on this thread's private condition variable
        shard->cv.wait(lock, [&]() noexcept {
          return stop_token_.load(std::memory_order_acquire) ||
                 !shard->tasks.empty();
        });

        if (stop_token_.load(std::memory_order_acquire))
          return;

        coroutine = shard->tasks.front();
        shard->tasks.pop_front();
      }

      if (coroutine && !coroutine.done()) {
        coroutine.resume();
      }
    }
  }
  // auto thread_loop(size_t index) noexcept -> void {
  //   while (!stop_token_.load(std::memory_order_acquire)) {
  //
  //     std::coroutine_handle<> coroutine;
  //
  //     {
  //       std::unique_lock lock(mutex_);
  //
  //       condition_variable_.wait(lock, [this]() noexcept {
  //         return stop_token_.load(std::memory_order_acquire) ||
  //                !coroutine_queue_.empty();
  //       });
  //
  //       if (stop_token_.load(std::memory_order_acquire))
  //         return;
  //
  //       coroutine = coroutine_queue_.front();
  //       coroutine_queue_.pop_front();
  //     }
  //
  //     if (coroutine && !coroutine.done())
  //       coroutine.resume();
  //   }
  // }

private:
  /// Mutex protecting the coroutine queue.
  // std::mutex mutex_;

  /// Condition variable used to wake worker threads.
  // std::condition_variable condition_variable_;

  /// Stop flag indicating pool shutdown.
  std::atomic<bool> stop_token_;

  std::vector<std::unique_ptr<WorkerQueue>> shards_;

  /// Queue of pending coroutine handles.
  // std::deque<std::coroutine_handle<>> coroutine_queue_;

  /// Worker threads executing scheduled coroutines.
  std::vector<std::thread> thread_list_;
  size_t thread_count_;
  std::atomic<size_t> next_{0}; // The Round-Robin counter
};
} // namespace net
