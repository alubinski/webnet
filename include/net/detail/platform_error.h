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
 * @brief Checks if a socket operation was interrupted by a signal.
 *
 * On Unix-like systems, checks for EINTR.
 * On Windows, always returns false (Windows does not use EINTR).
 *
 * @param err Error code to check.
 * @return true if the operation was interrupted, false otherwise.
 */
inline bool is_interrupted(int err) {
#ifdef _WIN32
  return false;
#else
  return err == EINTR;
#endif // _WIN32
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
