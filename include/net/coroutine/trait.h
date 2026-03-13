#pragma once

#include <concepts>
#include <coroutine>
namespace net {

/// @concept await_suspend_result
/// @brief Constrains the valid return types of an awaiter's `await_suspend`
/// method.
///
/// An awaiter's `await_suspend` method may return:
/// - `void` — execution resumes immediately after suspension.
/// - `bool` — `true` suspends the coroutine, `false` resumes immediately.
/// - `std::coroutine_handle<>` — transfers control to another coroutine.
///
/// @tparam T The type to check.
template <typename T>
concept await_suspend_result = std::same_as<T, void> || std::same_as<T, bool> ||
                               std::same_as<T, std::coroutine_handle<>>;
/// @concept awaiter
/// @brief Defines the requirements for a type to model a C++ awaiter.
///
/// A type satisfying this concept must provide:
/// - `bool await_ready()`
///   Returns whether the coroutine should suspend.
/// - `await_suspend(std::coroutine_handle<>)`
///   Called when suspending; must return a valid `await_suspend_result`.
/// - `await_resume()`
///   Produces the result of the await expression.
///
/// @note This concept validates the structural requirements only. It does not
///       enforce semantic correctness of coroutine behavior.
///
/// @tparam T The type to check.
template <typename T>
concept awaiter = requires(T t) {
  /// @brief Checks if the awaiter is ready without suspension.
  { t.await_ready() } -> std::same_as<bool>;

  /// @brief Handles coroutine suspension.
  /// @details Must return `void`, `bool`, or `std::coroutine_handle<>`.
  { t.await_suspend(nullptr) } -> await_suspend_result;

  /// @brief Returns the result of the await operation.
  { t.await_resume() };
};

/// @concept awaitable
/// @brief Determines whether a type can be used with the `co_await` operator.
///
/// A type is considered awaitable if it satisfies one of the following:
/// - Directly models the `awaiter` concept.
/// - Provides a member `operator co_await()` returning an awaiter.
/// - Provides a free function `operator co_await(T)` returning an awaiter.
///
/// @tparam T The type to check.
template <typename T>
concept awaitable = awaiter<T> || requires(T t) {
  { t.operator co_await() } -> awaiter;
} || requires(T t) {
  { operator co_await(t) } -> awaiter;
};

template <typename T>
  requires awaiter<T>
auto awaiter_t_impl() -> T;

template <typename T>
  requires awaitable<T>
auto awaiter_t_impl() -> decltype(std::declval<T>().operator co_await());

template <typename T>
  requires awaitable<T>
auto awaiter_t_impl() -> decltype(operator co_await(std::declval<T>()));

template <typename T> using awaiter_t = decltype(awaiter_t_impl<T>());

template <typename T>
  requires awaitable<T>
using await_result_t = decltype(std::declval<awaiter_t<T>>().await_resume());

} // namespace net
