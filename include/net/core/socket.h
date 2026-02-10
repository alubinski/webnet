#pragma once

#include <net/detail/socket_flags.h>
#include <net/detail/socket_handle.h>
#include <span>

namespace net::detail {

/**
 * @brief Base RAII wrapper around a native OS socket handle.
 *
 * Provides:
 *  - Ownership of the native handle
 *  - Cross-platform shutdown and close
 *  - Accessors for socket configuration flags
 *
 * Protocol-specific operations (send/recv) are implemented in derived classes.
 */
class Socket {
public:
  using Handle = SocketDescriptorHandle::Handle;

  /**
   * @brief Constructs a new socket with the specified flags.
   *
   * @param address_family IPv4/IPv6
   * @param socket_type    Stream/Dgram
   * @param protocol_type  TCP/UDP
   * @param blocking       Blocking or non-blocking
   * @param inheritable    Whether the handle is inheritable
   *
   * @throws std::system_error on failure
   */
  Socket(
      SocketFlags::AddressFamily address_family,
      SocketFlags::SocketType socket_type = SocketFlags::SocketType::Stream,
      SocketFlags::ProtocolType protocol_type = SocketFlags::ProtocolType::TCP,
      SocketFlags::BlockingType blocking = SocketFlags::BlockingType::Blocking,
      SocketFlags::InheritableType inheritable =
          SocketFlags::InheritableType::Inheritable);
  // Move-only

  Socket(Socket &&other) noexcept;
  Socket &operator=(Socket &&other) noexcept;

  Socket(const Socket &) = delete;
  Socket &operator=(const Socket &) = delete;

  /**
   * @brief Destructor closes the socket if valid.
   */
  ~Socket();

  /**
   * @brief Shutdown communication in the specified direction.
   */
  void shutdown(SocketFlags::ShutdownType how);

  /**
   * @brief Explicitly closes the socket.
   */
  void close() noexcept;

  /**
   * @brief Checks if the socket handle is valid.
   */
  [[nodiscard]] bool is_valid() const noexcept { return handle_.isValid(); }

  /**
   * @brief Returns the native OS socket handle.
   */
  [[nodiscard]] Handle native_handle() const noexcept { return handle_; }

  // --- Flag accessors ---

  [[nodiscard]] SocketFlags::AddressFamily address_family() const noexcept {
    return address_family_;
  }

  [[nodiscard]] SocketFlags::SocketType socket_type() const noexcept {
    return socket_type_;
  }

  [[nodiscard]] SocketFlags::ProtocolType protocol_type() const noexcept {
    return protocol_type_;
  }

  [[nodiscard]] SocketFlags::BlockingType blocking() const noexcept {
    return blocking_;
  }

  [[nodiscard]] SocketFlags::InheritableType inheritable() const noexcept {
    return inheritable_;
  }

protected:
  /**
   * @brief Constructs from an existing native handle (e.g., accept()).
   */
  Socket(Handle handle, SocketFlags::AddressFamily address_family,
         SocketFlags::SocketType socket_type,
         SocketFlags::ProtocolType protocol_type,
         SocketFlags::BlockingType blocking,
         SocketFlags::InheritableType inheritable) noexcept;

  /**
   * @brief Low-level send wrapper for derived classes.
   */
  [[nodiscard]] std::size_t raw_send(std::span<const std::byte> buffer);

  /**
   * @brief Low-level recv wrapper for derived classes.
   */
  [[nodiscard]] std::size_t raw_recv(std::span<std::byte> buffe);

  void setBlocking(const SocketFlags::BlockingType blocking_type);
  void setInheritable(const SocketFlags::InheritableType inheritable_type);

private:
  SocketDescriptorHandle handle_{};
  SocketFlags::AddressFamily address_family_;
  SocketFlags::SocketType socket_type_;
  SocketFlags::ProtocolType protocol_type_;
  SocketFlags::BlockingType blocking_;
  SocketFlags::InheritableType inheritable_;
};

} // namespace net::detail
