#include "net/detail/platform_error.h"
#include "net/detail/syscall_helpers.h"
#include <net/protocol/tcp/tcp_socket.h>
#include <system_error>

namespace net {

void TcpSocket::connect(const Endpoint &ep) {
  if (!is_valid()) {
    throw std::logic_error("connect on invalid socket");
  }
  int result;
  do {
    result = ::connect(native_handle(), ep.data(), ep.size());
  } while (result < 0 && detail::last_socket_error() == EINTR);

  if (result == 0) {
    return; // success
  }

  // Non-blocking connect in progress
  int err = detail::last_socket_error();
  if (blocking() == BlockingType::NonBlocking && detail::is_in_progress(err)) {
    return;
  }

  throw std::system_error(err, detail::socket_category(), "tcp connect failed");
}

void TcpSocket::bind(const Endpoint &ep) {
  if (!is_valid()) {
    throw std::logic_error("bind on invalid socket");
  }
  setReuseAddress(true);
  if (::bind(native_handle(), ep.data(), ep.size()) < 0) {
    throw std::system_error(detail::last_socket_error(),
                            detail::socket_category(), "tcp bind failed");
  }
}

void TcpSocket::setReuseAddress(bool enable) {
  if (!is_valid()) {
    throw std::logic_error("setReuseAddress on invalid socket");
  }
  int opt = enable ? 1 : 0;
  if (::setsockopt(native_handle(), SOL_SOCKET, SO_REUSEADDR,
                   reinterpret_cast<const char *>(&opt), sizeof(opt)) < 0) {

    throw std::system_error(detail::last_socket_error(),
                            detail::socket_category(),
                            "setsockopt(SO_REUSEADDR) failed");
  }
}

void TcpSocket::listen(int backlog) {
  if (!is_valid()) {
    throw std::logic_error("listen on invalid socket");
  }
  if (::listen(native_handle(), backlog) < 0) {
    throw std::system_error(detail::last_socket_error(),
                            detail::socket_category(), "tcp listen failed");
  }
}

TcpSocket TcpSocket::accept(Endpoint &peer) {
  if (!is_valid())
    throw std::logic_error("accept on invalid socket");

  auto new_handle = detail::retry_if_interrupted(
      [&] { return ::accept(native_handle(), peer.data(), peer.size_ptr()); });

  if (new_handle == detail::SocketDescriptorHandle::Invalid) {
    throw std::system_error(detail::last_socket_error(),
                            detail::socket_category(), "tcp accept failed");
  }
  return TcpSocket(new_handle, address_family(), blocking(), inheritable());
}

std::size_t TcpSocket::send(std::span<const std::byte> data) {
  return raw_send(data);
}
std::size_t TcpSocket::receive(std::span<std::byte> buffer) {
  return raw_recv(buffer);
}

Endpoint TcpSocket::localEndpoint() const {
  if (!is_valid()) {
    throw std::logic_error("localEndpoint on invalid socket");
  }

  Endpoint endpoint;

  detail::socket_length_t len = sizeof(sockaddr_storage);

  if (::getsockname(native_handle(), endpoint.data(), &len) < 0) {

    throw std::system_error(detail::last_socket_error(),
                            detail::socket_category(), "getsockname failed");
  }

  // Important: update internal size of Endpoint
  endpoint.set_size(len);

  return endpoint;
}

} // namespace net
