#define CATCH_CONFIG_MAIN
#include <catch2/catch_all.hpp>

#include "net/detail/socket.h"
#include "net/detail/socket_flags.h"

TEST_CASE("Socket creation") {
  net::detail::Socket sock(
      net::detail::SocketFlags::AddressFamily::IPV4,
      net::detail::SocketFlags::SocketType::Stream,
      net::detail::SocketFlags::ProtocolType::TCP,
      net::detail::SocketFlags::BlockingType::Blocking,
      net::detail::SocketFlags::InheritableType::NonInheritable);

  REQUIRE(sock.native_handle() != net::detail::SocketDescriptorHandle::Invalid);
}

TEST_CASE("Move semantics") {
  net::detail::Socket sock1(net::detail::SocketFlags::AddressFamily::IPV4,
                            net::detail::SocketFlags::SocketType::Stream,
                            net::detail::SocketFlags::ProtocolType::TCP);

  auto val = sock1.native_handle();
  net::detail::Socket sock2 = std::move(sock1);

  REQUIRE(sock1.native_handle() ==
          net::detail::SocketDescriptorHandle::Invalid);
  REQUIRE(sock2.native_handle() == val);
}
