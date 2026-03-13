#pragma once

#include "net/poll/ipoll.h"
#include <memory>

namespace net {

/**
 * @brief Creates a new I/O polling instance appropriate for the current
 * platform.
 *
 * This factory function handles the platform-specific selection of the
 * underlying I/O multiplexing mechanism (e.g., epoll, kqueue, or IOCP).
 *
 * @return std::unique_ptr<detail::IPoll> A unique pointer to the interface
 * implementation. The caller assumes full ownership of the returned object
 * and is responsible for its lifecycle.
 * * @note This is a factory method; the returned instance is ready for
 * event registration upon creation.
 */
std::unique_ptr<detail::IPoll> CreateDefaultReactor();

} // namespace net
