#pragma once

#ifdef _WIN32
#include <winsock2.h>
#else
#include <unistd.h>
#endif

namespace net::detail {

/**
 * @class SocketDescriptorHandle
 * @brief RAII wrapper for the native OS socket handle.
 *
 * This struct abstracts the platform-specific socket handle type:
 *  - Windows: Socket
 *  - Unix/Linux: int
 *
 * It provides basic ownership semantics, validity check, and release mechanism
 * for transferring ownership without closing the handle.
 *
 */
struct SocketDescriptorHandle {
#ifdef _WIN32
  /** @brief Native handle type on Windows (SOCKET). */
  using Handle = SOCKET;
  /** @brief Windows-specific invalid socket constant. */
  static constexpr Handle Invalid = INVALID_SOCKET;
#else
  /** @brief Native handle type on Unix-like systems (int). */
  using Handle = int;
  /** @brief Unix-specific invalid socket constant (-1). */
  static constexpr Handle Invalid = -1;
#endif

  Handle value = Invalid;

  /**
   * @brief Release ownership of handle without closing it.
   *
   * @return The raw handle.
   */
  Handle releaseHandle() noexcept {
    Handle old = value;
    value = Invalid;
    return old;
  }

  /**
   * @brief Checks whether the handle is valid (not equal to Invalid).
   *
   * @return true if valid, false otherwise.
   */
  [[nodiscard]] bool isValid() const noexcept { return value != Invalid; }

  /**
   * @brief Implicitly converts the handle wrapper to the native socket type.
   *
   * This allows the `SocketDescriptorHandle` to be passed directly to
   * OS-level socket functions (e.g., `::close()`, `::closesocket()`,
   * `::send()`,
   * `::recv()`).
   *
   * Note: This does **not** transfer ownership. The handle remains valid after
   * conversion. Use `releaseHandle()` to transfer ownership and invalidate
   * the wrapper.
   *
   * @return The underlying native socket handle.
   */
  operator Handle() const noexcept { return value; }

  SocketDescriptorHandle &operator=(Handle h) noexcept {
    value = h;
    return *this;
  }
};

} // namespace net::detail
