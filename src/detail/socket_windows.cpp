
#include "net/detail/socket.h"
#include "net/detail/socket_flags.h"
#include <system_error>
#include <winsock2.h>
#include <ws2tcpip.h>

namespace net::detail {

namespace {
struct WSAInitializer {
  WSAInitializer() {
    WSADATA data;
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
      throw std::runtime_error("WSAStartup failed");
    }
  }

  ~WSAInitializer() { WSACleanup(); }
};

static WSAInitializer wsa_init;
} // namespace

Socket::Socket(SocketFlags::AddressFamily address_family,
               SocketFlags::SocketType socket_type,
               SocketFlags::ProtocolType protocol_type,
               SocketFlags::BlockingType blocking,
               SocketFlags::InheritableType inheritable)
    : address_family_(address_family), socket_type_(socket_type),
      protocol_type_(protocol_type), blocking_(blocking),
      inheritable_(inheritable) {

  DWORD flags = WSA_FLAG_OVERLAPPED;
  if (inheritable == SocketFlags::InheritableType::NonInheritable) {
    flags |= WSA_FLAG_NO_HANDLE_INHERIT;
  }

  int domain = SocketFlags::toNative(address_family);
  int type = SocketFlags::toNative(socket_type);
  int protocol = SocketFlags::toNative(protocol_type_);

  handle_ = ::WSASocketW(domain, type, protocol, nullptr, 0, flags);

  if (!handle_.isValid()) {
    throw std::system_error(errno, std::generic_category(),
                            "WSASocketW() failed");
  }
  setBlocking(blocking);
}

void Socket::setBlocking(const SocketFlags::BlockingType blocking_type) {
  u_long enable =
      (blocking_type == SocketFlags::BlockingType::NonBlocking) ? 1ul : 0ul;

  if (::ioctlsocket(handle_, FIONBIO, &enable) == SOCKET_ERROR) {
    throw std::system_error(WSAGetLastError(), std::system_category(),
                            "ioctlsocket(FIONBIO) failed");
  }

  blocking_ = blocking_type;
}

void Socket::setInheritable(
    const SocketFlags::InheritableType inheritable_type) {
  DWORD flags = (inheritable_type == SocketFlags::InheritableType::Inheritable)
                    ? HANDLE_FLAG_INHERIT
                    : 0;
  HANDLE h = reinterpret_cast<HANDLE>(handle_.value);

  if (!::SetHandleInformation(h, HANDLE_FLAG_INHERIT, flags)) {
    throw std::system_error(GetLastError(), std::system_category(),
                            "SetHandleInformation failed");
  }

  inheritable_ = inheritable_type;
}

std::size_t Socket::raw_send(std::span<const std::byte> data) {
  int result = ::send(handle_.value, // SOCKET type
                      reinterpret_cast<const char *>(data.data()),
                      static_cast<int>(data.size()),
                      0 // no flags, MSG_NOSIGNAL not needed on Windows
  );

  if (result == SOCKET_ERROR) {
    throw std::system_error(WSAGetLastError(), std::system_category(),
                            "send() failed");
  }

  return static_cast<std::size_t>(result);
}

std::size_t Socket::raw_recv(std::span<std::byte> buffer) {
  int result = ::recv(handle_.value, reinterpret_cast<char *>(buffer.data()),
                      static_cast<int>(buffer.size()), 0);

  if (result == SOCKET_ERROR) {
    throw std::system_error(WSAGetLastError(), std::system_category(),
                            "recv() failed");
  }

  return static_cast<std::size_t>(result);
}

void Socket::shutdown(SocketFlags::ShutdownType how) {
  if (!handle_.isValid())
    return;

  int native_how = SocketFlags::toNative(how);

  if (::shutdown(handle_, native_how) == SOCKET_ERROR) {
    int err = WSAGetLastError();
    throw std::system_error(err, std::generic_category(), "shutdown() failed");
  }
}

void Socket::close() noexcept {
  ::closesocket(handle_);
  handle_ = SocketDescriptorHandle::Invalid;
}

} // namespace net::detail
