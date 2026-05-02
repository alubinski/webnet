#pragma once
#include "trait.h"
#include <atomic>
#include <coroutine>
#include <exception>
#include <variant>

enum class task_state : uint8_t { pending, completed, cancelled };

template <typename T = void> class wait_task_promise;

template <typename T = void> class wait_task {
public:
  using promise_type = wait_task_promise<T>;

  ~wait_task() {
    if (coroutine_) {
      coroutine_.destroy(); // safe: promise handles cancellation
    }
  }

  void wait() const noexcept {
    auto &promise = coroutine_.promise();
    auto &state = promise.state_;

    while (true) {
      auto s = state.load(std::memory_order_acquire);
      if (s != task_state::pending)
        break;

      state.wait(s, std::memory_order_acquire);
    }
  }

  bool cancelled() const noexcept {
    return coroutine_.promise().state_.load(std::memory_order_acquire) ==
           task_state::cancelled;
  }

  template <typename Self> decltype(auto) get_return_value(this Self &&self) {
    return std::forward<Self>(self).coroutine_.promise().get_return_value();
  }

private:
  friend promise_type;

  explicit wait_task(std::coroutine_handle<promise_type> h) noexcept
      : coroutine_(h) {}

  std::coroutine_handle<promise_type> coroutine_;
};

template <typename T = void> class wait_task_promise_base {
public:
  using handle_t = std::coroutine_handle<wait_task_promise<T>>;

  wait_task_promise_base() noexcept : state_(task_state::pending) {}

  auto initial_suspend() noexcept { return std::suspend_never{}; }

  auto final_suspend() noexcept {
    struct awaiter {
      bool await_ready() noexcept { return false; }

      void await_suspend(handle_t h) noexcept {
        auto &promise = h.promise();
        promise.state_.store(task_state::completed, std::memory_order_release);
        promise.state_.notify_all();
      }

      void await_resume() noexcept {}
    };
    return awaiter{};
  }

  [[noreturn]] void unhandled_exception() noexcept { std::terminate(); }

  void cancel() noexcept {
    auto expected = task_state::pending;
    if (state_.compare_exchange_strong(expected, task_state::cancelled,
                                       std::memory_order_release)) {
      state_.notify_all();
    }
  }

  std::atomic<task_state> state_;

private:
};

template <>
class wait_task_promise<void> : public wait_task_promise_base<void> {
public:
  auto get_return_object() noexcept {
    return wait_task<void>{
        std::coroutine_handle<wait_task_promise>::from_promise(*this)};
  }

  void return_void() noexcept {}
};

template <typename T>
class wait_task_promise : public wait_task_promise_base<T> {
public:
  auto get_return_object() noexcept {
    return wait_task<T>{
        std::coroutine_handle<wait_task_promise>::from_promise(*this)};
  }

  template <typename U>
    requires std::convertible_to<U &&, T>
  void
  return_value(U &&value) noexcept(std::is_nothrow_convertible_v<U &&, T>) {
    return_value_.emplace(std::forward<U>(value));
  }

  T &get_return_value() & { return std::get<T>(return_value_); }

private:
  std::variant<std::monostate, T> return_value_;
};

// template <typename T = void> class wait_task_promise;
//
// template <typename T = void> class wait_task {
// public:
//   using promise_type = wait_task_promise<T>;
//
//   ~wait_task() { coroutine_.destroy(); }
//
//   auto wait() const noexcept -> void {
//     coroutine_.promise().get_stop_token().wait(false,
//                                                std::memory_order_acquire);
//   }
//
//   template <typename Self>
//   [[nodiscard]]
//   auto get_return_value(this Self &&self) noexcept -> decltype(auto) {
//     return std::forward<Self>(self).coroutine_.promise().get_return_value();
//   }
//
// private:
//   friend promise_type;
//
//   explicit wait_task(
//       const std::coroutine_handle<wait_task_promise<T>> coroutine) noexcept
//       : coroutine_{coroutine} {}
//
//   const std::coroutine_handle<wait_task_promise<T>> coroutine_;
// };
//
// template <typename T = void> class wait_task_promise_base {
// public:
//   class final_awaiter {
//   public:
//     [[nodiscard]] auto await_ready() const noexcept -> bool { return false; }
//
//     auto await_resume() const noexcept -> void {}
//
//     auto await_suspend(const std::coroutine_handle<wait_task_promise<T>>
//                            coroutine) const noexcept -> void {
//       std::atomic_flag &stop_token_ = coroutine.promise().get_stop_token();
//       stop_token_.test_and_set(std::memory_order_release);
//       stop_token_.notify_one();
//     }
//   };
//
//   [[nodiscard]] auto initial_suspend() const noexcept -> std::suspend_never {
//     return std::suspend_never{};
//   }
//
//   [[nodiscard]] auto final_suspend() const noexcept -> final_awaiter {
//     return final_awaiter{};
//   }
//
//   [[noreturn]] auto unhandled_exception() const noexcept -> void {
//     std::terminate();
//   }
//
//   auto get_stop_token() noexcept -> std::atomic_flag & { return stop_token_;
//   }
//
// private:
//   std::atomic_flag stop_token_;
// };
//
// template <>
// class wait_task_promise<void> final : public wait_task_promise_base<void> {
// public:
//   [[nodiscard]] auto get_return_object() noexcept -> wait_task<void> {
//     return wait_task<void>{
//         std::coroutine_handle<wait_task_promise>::from_promise(*this)};
//   };
//
//   auto return_void() const noexcept -> void {}
// };
//
// template <typename T>
// class wait_task_promise final : public wait_task_promise_base<T> {
// public:
//   [[nodiscard]] auto get_return_object() noexcept -> wait_task<T> {
//     return wait_task{
//         std::coroutine_handle<wait_task_promise>::from_promise(*this)};
//   }
//
//   template <typename U>
//     requires std::convertible_to<U &&, T>
//   auto return_value(U &&return_value) noexcept(
//       std::is_nothrow_convertible_v<U &&, T>) -> void {
//     return_value_.emplace(std::forward<U>(return_value));
//   }
//
//   template <typename Self>
//   [[nodiscard]]
//   auto get_return_value(this Self &&self) noexcept -> decltype(auto) {
//     return *(std::forward<Self>(self).return_value_);
//   }
//
// private:
//   std::variant<std::monostate, T> return_value_;
//   // deferred_initialization<T> return_value_;
// };
//
template <typename Awaitable> auto wait(Awaitable &&awaitable) {
  auto wait_task_promise =
      [&awaitable]() noexcept -> wait_task<net::await_result_t<Awaitable>> {
    co_return co_await std::forward<Awaitable>(awaitable);
  }();
  wait_task_promise.wait();

  if constexpr (!std::same_as<net::await_result_t<Awaitable>, void>) {
    return wait_task_promise.get_return_value();
  }
}
