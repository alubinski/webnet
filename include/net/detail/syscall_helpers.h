#pragma once
#include "platform_error.h"

namespace net::detail {

/**
 * @brief Retries a socket operation if it is interrupted by a signal.
 *
 * This helper repeatedly calls the provided function until it either
 * succeeds or fails with an error other than an interrupt (EINTR on Unix).
 *
 * On Windows, `is_interrupted` always returns false, so the function
 * is called only once.
 *
 * @tparam F Callable type, e.g., a lambda performing a socket call.
 * @param func Function to execute. Must return an integer (socket descriptor,
 * length, or -1 on error).
 * @return The result of the function call after a successful non-interrupted
 * execution.
 */
template <typename F> auto retry_if_interrupted(F &&func) {
  decltype(func()) result;

  do {
    result = func();
  } while (result == -1 && detail::is_interrupted(detail::last_socket_error()));

  return result;
}

} // namespace net::detail
