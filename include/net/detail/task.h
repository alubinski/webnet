#pragma once
#include <coroutine>
#include <exception>
#include <optional>
#include <stdexcept>
#include <utility>

namespace net {

template <typename T = void> class task;

//
// =======================
// promise_type (generic T)
// =======================
//

/**
 * @brief Coroutine task type representing an asynchronous computation.
 *
 * `net::task<T>` is a lightweight coroutine wrapper that models a lazy
 * asynchronous operation producing a result of type `T`.
 *
 * The coroutine:
 * - starts suspended (`initial_suspend`)
 * - resumes when awaited or when `get()` is called
 * - stores either a result (`T`) or an exception
 * - resumes its continuation at `final_suspend`
 *
 * @tparam T Result type of the asynchronous computation.
 */
template <typename T> class task {
public:
  /**
   * @brief Promise type for `task<T>`.
   *
   * Manages coroutine state, result storage, exception propagation,
   * and continuation handling.
   */
  struct promise_type {

    /// stored result value
    std::optional<T> value_;

    /// stored exception (if any)
    std::exception_ptr exception_;

    /// Continuation coroutine resumed at final suspend.
    std::coroutine_handle<> continuation_;

    /**
     * @brief Creates the associated task object.
     */
    task get_return_object() noexcept {
      return task{std::coroutine_handle<promise_type>::from_promise(*this)};
    }

    /**
     * @brief Always suspend initially (lazy execution).
     */
    std::suspend_always initial_suspend() noexcept { return {}; }

    /**
     * @brief Always suspend initially (lazy execution).
     */
    auto final_suspend() noexcept {
      struct awaiter {
        bool await_ready() noexcept { return false; }

        void await_suspend(std::coroutine_handle<promise_type> h) noexcept {
          if (h.promise().continuation_) {
            h.promise().continuation_.resume();
          }
        }

        void await_resume() noexcept {}
      };
      return awaiter{};
    }

    /**
     * @brief Stores an unhandled exception.
     */
    void unhandled_exception() noexcept {
      exception_ = std::current_exception();
    }

    /**
     * @brief Stores the return value.
     */
    template <typename U>
    void
    return_value(U &&value) noexcept(std::is_nothrow_constructible_v<T, U &&>) {
      value_ = std::forward<U>(value);
    }
  };

  /// Coroutine handle type.
  using handle_type = std::coroutine_handle<promise_type>;

  /**
   * @brief Constructs an empty task.
   */
  task() noexcept = default;

  /**
   * @brief Constructs a task from coroutine handle.
   */
  explicit task(handle_type handle) noexcept : handle_(handle) {}

  /**
   * @brief Move constructor.
   */
  task(task &&other) noexcept : handle_(std::exchange(other.handle_, {})) {}

  /**
   * @brief Move assignment.
   */
  task &operator=(task &&other) noexcept {
    if (this != &other) {
      if (handle_)
        handle_.destroy();
      handle_ = std::exchange(other.handle_, {});
    }
    return *this;
  }

  /**
   * @brief Destroys the coroutine if still owned.
   */
  ~task() {
    if (handle_)
      handle_.destroy();
  }

  /**
   * @brief Returns true if coroutine is already completed.
   */
  bool await_ready() const noexcept { return !handle_ || handle_.done(); }

  /**
   * @brief Suspends awaiting coroutine and resumes this task.
   *
   * @param continuation The awaiting coroutine handle.
   */
  void await_suspend(std::coroutine_handle<> continuation) noexcept {
    handle_.promise().continuation_ = continuation;
    handle_.resume();
  }

  /**
   * @brief Retrieves the result or rethrows stored exception.
   *
   * @return Result of the coroutine.
   */
  T await_resume() {
    auto &promise = handle_.promise();

    if (promise.exception_)
      std::rethrow_exception(promise.exception_);

    return std::move(*promise.value_);
  }

  /**
   * @brief Synchronously executes the coroutine to completion.
   *
   * Repeatedly resumes the coroutine until finished.
   *
   * @return Result of the coroutine.
   *
   * @throws Stored exception if coroutine failed.
   * @throws std::runtime_error if task is invalid or has no result.
   */
  T get() {
    if (!handle_)
      throw std::runtime_error("invalid task");

    while (!handle_.done()) {
      handle_.resume();
    }

    auto &promise = handle_.promise();

    if (promise.exception_)
      std::rethrow_exception(promise.exception_);

    if (!promise.value_.has_value())
      throw std::runtime_error("task has no result");

    return std::move(*promise.value_);
  }

private:
  handle_type handle_;
};

/**
 * @brief Specialization of task for `void` result type.
 *
 * Represents an asynchronous operation that does not produce a value.
 */
template <> class task<void> {
public:
  struct promise_type {

    /// Stored exception (if any).
    std::exception_ptr exception_;

    /// Stored exception (if any).
    std::coroutine_handle<> continuation_;

    /**
     * @brief Creates the associated task object.
     */
    task get_return_object() noexcept {
      return task{std::coroutine_handle<promise_type>::from_promise(*this)};
    }

    /**
     * @brief Always suspend initially (lazy execution).
     */
    std::suspend_always initial_suspend() noexcept { return {}; }

    /**
     * @brief Final suspend point.
     *
     * Resumes the awaiting coroutine (if any).
     */
    auto final_suspend() noexcept {
      struct awaiter {
        bool await_ready() noexcept { return false; }

        void await_suspend(std::coroutine_handle<promise_type> h) noexcept {
          if (h.promise().continuation_) {
            h.promise().continuation_.resume();
          }
        }

        void await_resume() noexcept {}
      };
      return awaiter{};
    }

    /**
     * @brief Stores an unhandled exception.
     */
    void unhandled_exception() noexcept {
      exception_ = std::current_exception();
    }

    /**
     * @brief Handles `co_return` with no value.
     */
    void return_void() noexcept {}
  };

  using handle_type = std::coroutine_handle<promise_type>;

  task() noexcept = default;

  explicit task(handle_type handle) noexcept : handle_(handle) {}

  task(task &&other) noexcept : handle_(std::exchange(other.handle_, {})) {}

  task &operator=(task &&other) noexcept {
    if (this != &other) {
      if (handle_)
        handle_.destroy();
      handle_ = std::exchange(other.handle_, {});
    }
    return *this;
  }

  ~task() {
    if (handle_)
      handle_.destroy();
  }

  /**
   * @brief Returns true if coroutine is already completed.
   */
  bool await_ready() const noexcept { return !handle_ || handle_.done(); }

  /**
   * @brief Suspends awaiting coroutine and resumes this task.
   */
  void await_suspend(std::coroutine_handle<> continuation) noexcept {
    handle_.promise().continuation_ = continuation;
    handle_.resume();
  }

  /**
   * @brief Rethrows stored exception if coroutine failed.
   */
  void await_resume() {
    auto &promise = handle_.promise();

    if (promise.exception_)
      std::rethrow_exception(promise.exception_);
  }

  /**
   * @brief Synchronously executes the coroutine to completion.
   *
   * @throws Stored exception if coroutine failed.
   * @throws std::runtime_error if task is invalid.
   */
  void get() {
    if (!handle_)
      throw std::runtime_error("invalid task");

    while (!handle_.done()) {
      handle_.resume();
    }

    auto &promise = handle_.promise();

    if (promise.exception_)
      std::rethrow_exception(promise.exception_);
  }

private:
  handle_type handle_;
};

} // namespace net
