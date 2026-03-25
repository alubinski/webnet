#pragma once
#include <coroutine>
#include <exception>
#include <utility>
#include <variant>

template <typename T = void> class task_promise;

/**
 * @brief Coroutine task wrapper representing an asynchronous computation.
 *
 * `task<T>` is a lazy coroutine abstraction. The coroutine does not start
 * executing until it is either:
 * - awaited (`co_await task`)
 * - explicitly started using @ref detach().
 *
 * The object owns the coroutine handle and destroys it on destruction
 * unless ownership has been relinquished (e.g., via detach).
 *
 * @tparam T Result type produced by the coroutine. Use `void` for no result.
 */
template <typename T = void> class task {
public:
  /// Associated promise type used by the coroutine machinery.
  using promise_type = task_promise<T>;

  /// Coroutine handle type bound to the promise.
  using handle_type = std::coroutine_handle<promise_type>;

  /**
   * @brief Construct a task from a coroutine handle.
   *
   * Typically called by the coroutine machinery via
   * `promise_type::get_return_object()`.
   *
   * @param h Coroutine handle.
   */
  explicit task(handle_type h) : handle(h) {}

  /**
   * @brief Move constructor.
   *
   * Transfers coroutine ownership from another task.
   */
  task(task &&other) noexcept : handle(std::exchange(other.handle, nullptr)) {}

  /**
   * @brief Move assignment operator.
   *
   * Destroys the currently owned coroutine (if any) and takes ownership
   * from another task.
   */
  task &operator=(task &&other) noexcept {
    if (this != &other) {
      if (handle)
        handle.destroy();
      handle = std::exchange(other.handle, nullptr);
    }
    return *this;
  }

  /**
   * @brief Destructor.
   *
   * Destroys the coroutine if it is still owned by this task.
   */
  ~task() {
    if (handle)
      handle.destroy();
  }

  /**
   * @brief Start the coroutine in detached mode.
   *
   * Marks the coroutine as detached and resumes it immediately.
   * After detaching, the task relinquishes ownership of the coroutine.
   *
   * When the coroutine completes, it will destroy itself automatically
   * in `final_suspend`.
   */
  void detach() {
    if (handle) {
      handle.promise().detached = true;
      handle.resume();  // Kick off the lazy coroutine
      handle = nullptr; // Relinquish ownership
    }
  }

  std::coroutine_handle<promise_type> take_handle() {
    // 1. Grab the current handle
    auto h = handle;

    // 2. Set the internal handle to nullptr so the
    // Task destructor doesn't kill the coroutine.
    handle = nullptr;

    // 3. Return the handle to the caller (the ThreadPool)
    return h;
  }

  /**
   * @brief Awaiter readiness check.
   *
   * @return true if the coroutine is already finished or null.
   */
  bool await_ready() const noexcept { return !handle || handle.done(); }

  /**
   * @brief Suspends the awaiting coroutine and transfers control.
   *
   * The awaiting coroutine becomes the continuation of this task.
   *
   * @param cont Handle of the awaiting coroutine.
   * @return Handle of the coroutine to resume (symmetric transfer).
   */
  std::coroutine_handle<> await_suspend(std::coroutine_handle<> cont) noexcept {
    handle.promise().continuation = cont;
    return handle; // Symmetric transfer
  }
  /**
   * @brief Resume the awaiting coroutine and retrieve the result.
   *
   * @return Result produced by the coroutine.
   */
  T await_resume() { return handle.promise().get_result(); }

private:
  /// Underlying coroutine handle.
  handle_type handle;
};

/**
 * @brief Base class shared by all task promise specializations.
 *
 * Stores coroutine continuation and exception state. Provides
 * common suspend behavior and the final awaiter.
 */
struct task_promise_base {
  /// Continuation coroutine awaiting this task.
  std::coroutine_handle<> continuation;

  /// Captured exception thrown inside the coroutine.
  std::exception_ptr exception;

  /// Whether the coroutine is running detached.
  bool detached = false;

  /**
   * @brief Initial suspend point.
   *
   * Makes the coroutine lazy: execution begins only when resumed.
   */
  std::suspend_always initial_suspend() noexcept { return {}; }

  /// @return Detached flag state.
  bool get_detached_flag() const noexcept { return detached; }

  /// @return Stored continuation handle.
  std::coroutine_handle<> get_continuation() const noexcept {
    return continuation;
  }

  /**
   * @brief Awaiter used at coroutine final suspension.
   *
   * Determines what happens when the coroutine finishes execution.
   */
  struct final_awaiter {
    /// Always suspend at final suspend.
    bool await_ready() noexcept { return false; }

    /**
     * @brief Transfers execution after coroutine completion.
     *
     * - If detached → destroy coroutine immediately.
     * - If a continuation exists → resume it.
     * - Otherwise → return noop coroutine.
     */
    template <typename P>
    std::coroutine_handle<> await_suspend(std::coroutine_handle<P> h) noexcept {
      // Your logic: If detached, destroy and stop. Otherwise, resume parent.
      if (h.promise().get_detached_flag()) {
        h.destroy();
        return std::noop_coroutine();
      }

      if (auto cont = h.promise().get_continuation()) {
        return cont;
      }

      return std::noop_coroutine();
    }

    /// No result returned from final suspension.
    void await_resume() noexcept {}
  };

  /**
   * @brief Final suspend point of the coroutine.
   */
  final_awaiter final_suspend() noexcept { return {}; }

  /**
   * @brief Store unhandled exception thrown in coroutine.
   */
  void unhandled_exception() { exception = std::current_exception(); }
};

/**
 * @brief Promise type for `task<T>` coroutines.
 *
 * Stores the result of type `T` and exposes it to the awaiting coroutine.
 *
 * @tparam T Result type.
 */
template <typename T> struct task_promise final : public task_promise_base {
  /// Result storage.
  std::variant<std::monostate, T> result;

  /**
   * @brief Create the associated task object.
   */
  task<T> get_return_object() {
    return task<T>{std::coroutine_handle<task_promise>::from_promise(*this)};
  }

  /**
   * @brief Store returned value (rvalue).
   */
  void return_value(T &&value) { result.template emplace<1>(std::move(value)); }

  /**
   * @brief Store returned value (lvalue).
   */
  void return_value(const T &value) { result.template emplace<1>(value); }

  /**
   * @brief Retrieve coroutine result.
   *
   * @throws Re-throws stored exception if coroutine failed.
   */
  T get_result() {
    if (exception)
      std::rethrow_exception(exception);
    return std::move(std::get<1>(result));
  }
};

/**
 * @brief Promise specialization for `task<void>`.
 */
template <> struct task_promise<void> final : public task_promise_base {

  /**
   * @brief Create the associated task object.
   */
  task<void> get_return_object() {
    return task<void>{std::coroutine_handle<task_promise>::from_promise(*this)};
  }

  /**
   * @brief Coroutine return for `void` result.
   */
  void return_void() {}

  /**
   * @brief Resume awaiting coroutine and propagate exception if any.
   */
  void get_result() {
    if (exception)
      std::rethrow_exception(exception);
  }
};
