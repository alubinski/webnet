#include "net/core/socket.h"
#include "net/detail/platform_error.h"
#include "net/detail/socket_flags.h"
#include "net/detail/socket_handle.h"
#include <arpa/inet.h> // inet_pton
#include <cerrno>
#include <errno.h> // errno
#include <fcntl.h> // fcntl
#include <netdb.h> // AF_INET / IPPROTO_TCP / AF_UNSPEC
#include <sys/socket.h>
#include <system_error>
#include <unistd.h> // close

namespace net::detail {

Socket::Socket(SocketFlags::AddressFamily address_family,
               SocketFlags::SocketType socket_type,
               SocketFlags::ProtocolType protocol_type,
               SocketFlags::BlockingType blocking,
               SocketFlags::InheritableType inheritable)
    : address_family_(address_family), socket_type_(socket_type),
      protocol_type_(protocol_type), blocking_(blocking),
      inheritable_(inheritable) {

  int type_flags = SocketFlags::toNative(socket_type);
#ifdef SOCK_NONBLOCK
  if (blocking == SocketFlags::BlockingType::NonBlocking) {
    type_flags |= SOCK_NONBLOCK;
  }
#endif
#ifdef SOCK_CLOEXEC
  if (inheritable == SocketFlags::InheritableType::NonInheritable) {
    type_flags |= SOCK_CLOEXEC;
  }
#endif

  int domain = SocketFlags::toNative(address_family_);
  int protocol = SocketFlags::toNative(protocol_type_);

  do {
    handle_ = ::socket(domain, type_flags, protocol);
  } while (!handle_.isValid() && errno == EINTR);

  if (!handle_.isValid()) {
    throw std::system_error(errno, std::generic_category(), "socket() failed");
  }

#ifndef SOCK_NONBLOCK
  if (blocking == SocketFlags::BlockingType::NonBlocking) {
    setBlocking(blocking);
  }
#endif
#ifndef SOCK_CLOEXEC
  if (inheritable == SocketFlags::InheritableType::NonInheritable) {
    setInheritable(inheritable);
  }
#endif

#ifdef SO_NOSIGPIPE
  {
    int active = 1;
    ::setsockopt(handle_, SOL_SOCKET, SO_NOSIGPIPE, &active, sizeof(active));
  }
#endif // defined(SO_NOSIGPIPE)
}

void Socket::setBlocking(const SocketFlags::BlockingType blocking_type) {
  int flags = ::fcntl(handle_, F_GETFL, 0);
  if (flags < 0) {
    throw std::runtime_error("fcntl(F_GETFL) failed");
  }

  if (blocking_type == SocketFlags::BlockingType::NonBlocking) {
    flags |= O_NONBLOCK;
  } else {
    flags &= ~O_NONBLOCK;
  }

  if (::fcntl(handle_, F_SETFL, flags) < 0) {
    throw std::system_error(errno, std::generic_category(),
                            "fcntl(F_GETFL) failed");
  }
  blocking_ = blocking_type;
}

void Socket::setInheritable(
    const SocketFlags::InheritableType inheritable_type) {
  int flags = ::fcntl(handle_, F_GETFD);

  if (flags < 0) {
    throw std::system_error(errno, std::generic_category(),
                            "fcntl(F_GETFD) failed");
  }

  if (inheritable_type == SocketFlags::InheritableType::NonInheritable)
    flags |= FD_CLOEXEC;
  else
    flags &= ~FD_CLOEXEC;

  if (::fcntl(handle_, F_SETFD, flags) < 0) {
    throw std::system_error(errno, std::generic_category(),
                            "fcntl(F_SETFD) failed");
  }

  inheritable_ = inheritable_type;
}

std::size_t Socket::raw_send(std::span<const std::byte> data) {
  if (!is_valid()) {
    throw std::runtime_error("send on invalid socket");
  }

  for (;;) {
    const auto result = ::send(
        handle_, reinterpret_cast<const void *>(data.data()), data.size(),
#ifdef MSG_NOSIGNAL
        MSG_NOSIGNAL
#else
        0
#endif
    );

    if (result >= 0)
      return static_cast<std::size_t>(result);

    const int err = errno;

    if (is_interrupted(err))
      continue;

    if (is_would_block(err)) {
      if (blocking() == SocketFlags::BlockingType::NonBlocking) {
        return 0; // signal to async layer
      }
      continue;
    }

    throw std::system_error(err, std::generic_category(), "send() failed");
  }
}

std::size_t Socket::raw_recv(std::span<std::byte> buffer) {
  if (!is_valid()) {
    throw std::runtime_error("recv invalid socket");
  }

  for (;;) {
    const auto result = ::recv(handle_, reinterpret_cast<void *>(buffer.data()),
                               buffer.size(), 0);

    if (result >= 0)
      return static_cast<std::size_t>(result);

    const int err = errno;

    if (is_interrupted(err))
      continue;

    if (is_would_block(err)) {
      if (blocking() == SocketFlags::BlockingType::NonBlocking) {
        return 0; // signal to async layer
      }
      continue;
    }

    throw std::system_error(err, std::generic_category(), "recv() failed");
  }
}

void Socket::shutdown(SocketFlags::ShutdownType how) {
  if (!handle_.isValid())
    return;

  int native_how = SocketFlags::toNative(how);

  if (::shutdown(handle_, native_how) < 0) {
    throw std::system_error(errno, std::generic_category(),
                            "shutdown() failed");
  }
}

void Socket::close() noexcept {
  ::close(handle_);
  handle_ = SocketDescriptorHandle::Invalid;
}

} // namespace net::detail
