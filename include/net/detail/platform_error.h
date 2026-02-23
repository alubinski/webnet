#pragma once
#include <cerrno>
#include <system_error>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

namespace net::detail {
/**
 * @brief Returns the last socket error code for the current thread.
 *
 * On Windows, wraps WSAGetLastError().
 * On Unix-like systems, returns errno.
 *
 * @return Platform-specific error code.
 */
inline int last_socket_error() noexcept {
#ifdef _WIN32
  return WSAGetLastError();
#else
  return errno;
#endif
}

/**
 * @brief Checks if a socket operation is in progress.
 *
 * Useful for non-blocking connect operations.
 *
 * @param err Error code to check.
 * @return true if the operation is still in progress, false otherwise.
 */
inline bool is_in_progress(int err) {
#ifdef _WIN32
  return err == WSAEWOULDBLOCK;
#else
  return err == EINPROGRESS;
#endif // _WIN32
}

/**
 * @brief Checks whether a socket operation was interrupted.
 *
 * This function tests if the provided error code corresponds to an interrupt
 * condition that typically occurs when a blocking operation is interrupted by a
 * signal or event.
 *
 * Platform-specific behavior:
 * - On Windows, checks against `WSAEINTR`.
 * - On Unix-like systems, checks against `EINTR`.
 *
 * @param err Error code to evaluate.
 * @return true if the error indicates an interrupted operation, false
 * otherwise.
 */
inline bool is_interrupted(int err) {
#ifdef _WIN32
  return err == WSAEINTR;
#else
  return err == EINTR;
#endif // _WIN32
}

/**
 * @brief Checks whether the given error code represents a non-blocking "would
 * block" condition.
 *
 * This function determines if a system error corresponds to a situation where
 * an operation cannot be completed immediately because it would block. The
 * check is platform-dependent:
 *
 * - On Windows, it compares against `WSAEWOULDBLOCK`.
 * - On non-Windows systems, it checks for `EAGAIN` or `EWOULDBLOCK`.
 *
 * @param err The error code to evaluate.
 * @return True if the error indicates a "would block" condition, otherwise
 * false.
 */
inline bool is_would_block(int err) {
#ifdef _WIN32
  return err == WSAEWOULDBLOCK;
#else
  return err == EAGAIN || err == EWOULDBLOCK;
#endif
}

/**
 * @brief Returns the std::error_category for socket errors.
 *
 * On Windows, uses std::system_category().
 * On Unix-like systems, uses std::generic_category().
 *
 * @return Reference to the error_category for socket errors.
 */
inline const std::error_category &socket_category() noexcept {
#ifdef _WIN32
  return std::system_category();
#else
  return std::generic_category();
#endif // _WIN32
}
} // namespace net::detail
