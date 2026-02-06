#include "net/detail/socket.h"
#include "net/detail/socket_handle.h"
namespace net::detail {

Socket::Socket(Handle handle, SocketFlags::AddressFamily address_family,
               SocketFlags::SocketType socket_type,
               SocketFlags::ProtocolType protocol_type,
               SocketFlags::BlockingType blocking,
               SocketFlags::InheritableType inheritable) noexcept
    : address_family_(address_family), socket_type_(socket_type),
      protocol_type_(protocol_type), blocking_(blocking),
      inheritable_(inheritable), handle_(handle) {}

Socket::Socket(Socket &&other) noexcept
    : address_family_(other.address_family_), socket_type_(other.socket_type_),
      protocol_type_(other.protocol_type_), blocking_(other.blocking_),
      inheritable_(other.inheritable_), handle_(other.handle_.releaseHandle()) {
}

Socket &Socket::operator=(Socket &&other) noexcept {
  if (this != &other) {
    // Close current socket safely using class method
    close();

    address_family_ = other.address_family_;
    socket_type_ = other.socket_type_;
    protocol_type_ = other.protocol_type_;
    blocking_ = other.blocking_;
    inheritable_ = other.inheritable_;

    handle_ = other.handle_.releaseHandle();
  }
  return *this;
}

Socket::~Socket() { close(); }

} // namespace net::detail
