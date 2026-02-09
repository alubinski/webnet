#pragma once

#ifndef _WIN32
#include "sys/socket.h"
#endif // !_WIN32

namespace net::detail {
/**
 * @brief Platform-specific type for socket address length.
 *
 * Used in socket functions that require the length of a sockaddr
 * structure, e.g., `accept`, `bind`, `connect`, `getsockname`.
 *
 * - On Windows: `int`
 * - On Unix/Linux/macOS: `socklen_t`
 */
#ifdef _WIN32
using socket_length_t = int;
#else
using socket_length_t = socklen_t;
#endif
} // namespace net::detail
