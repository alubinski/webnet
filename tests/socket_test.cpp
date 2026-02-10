#include "catch2/catch_test_macros.hpp"
#define CATCH_CONFIG_MAIN
#include <catch2/catch_all.hpp>

#include "net/core/socket.h"
#include "net/detail/socket_flags.h"

using namespace net::detail;

class TestSocket : public net::detail::Socket {
public:
  using Socket::Socket;
  TestSocket(SocketDescriptorHandle handle, SocketFlags::AddressFamily af,
             SocketFlags::SocketType st, SocketFlags::ProtocolType pt,
             SocketFlags::BlockingType bt = SocketFlags::BlockingType::Blocking,
             SocketFlags::InheritableType it =
                 SocketFlags::InheritableType::Inheritable)
      : Socket(handle, af, st, pt, bt, it) {}

  using Socket::close;
  using Socket::raw_recv;
  using Socket::raw_send;
  using Socket::setBlocking;
  using Socket::setInheritable;
  using Socket::shutdown;
};

static constexpr auto AF = net::detail::SocketFlags::AddressFamily::IPV4;
static constexpr auto BLOCK = net::detail::SocketFlags::BlockingType::Blocking;
static constexpr auto NONBLOCK =
    net::detail::SocketFlags::BlockingType::NonBlocking;
static constexpr auto INHERIT =
    net::detail::SocketFlags::InheritableType::Inheritable;
static constexpr auto NOINHERIT =
    net::detail::SocketFlags::InheritableType::NonInheritable;

TEST_CASE("Socket creation") {
  TestSocket sock(AF, SocketFlags::SocketType::Stream,
                  SocketFlags::ProtocolType::TCP, BLOCK, NOINHERIT);

  REQUIRE(sock.native_handle() != SocketDescriptorHandle::Invalid);
  REQUIRE(sock.is_valid());
}

TEST_CASE("Move semantics") {
  TestSocket sock1(AF, SocketFlags::SocketType::Stream,
                   SocketFlags::ProtocolType::TCP);

  auto val = sock1.native_handle();
  TestSocket sock2 = std::move(sock1);

  REQUIRE_FALSE(sock1.is_valid());
  REQUIRE(sock2.native_handle() == val);
}

TEST_CASE("Move assigment semantics") {
  TestSocket sock1(AF, SocketFlags::SocketType::Stream,
                   SocketFlags::ProtocolType::TCP);

  TestSocket sock2(AF, SocketFlags::SocketType::Stream,
                   SocketFlags::ProtocolType::TCP);

  auto old_handle = sock1.native_handle();

  sock2 = std::move(sock1);

  REQUIRE_FALSE(sock1.is_valid());
  REQUIRE(sock2.native_handle() == old_handle);
}

TEST_CASE("Base close invalidates descriptor") {
  TestSocket s(AF, SocketFlags::SocketType::Stream,
               SocketFlags::ProtocolType::TCP);

  s.close();

  REQUIRE_FALSE(s.is_valid());
}

TEST_CASE("Using moved-from socket throws") {
  TestSocket s1(AF, SocketFlags::SocketType::Stream,
                SocketFlags::ProtocolType::TCP);

  TestSocket s2 = std::move(s1);

  std::vector<std::byte> buffer(4);

  REQUIRE_THROWS(s1.raw_recv(buffer));
}

#ifndef _WIN32
#include <fcntl.h>
#include <sys/socket.h>

TEST_CASE("Non-blocking raw_recv returns EAGAIN") {
  int fds[2];
  REQUIRE(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);

  TestSocket s1(SocketDescriptorHandle{fds[0]}, AF,
                SocketFlags::SocketType::Stream, SocketFlags::ProtocolType::TCP,
                NONBLOCK, NOINHERIT);

  TestSocket s2(SocketDescriptorHandle{fds[1]}, AF,
                SocketFlags::SocketType::Stream,
                SocketFlags::ProtocolType::TCP);

  std::vector<std::byte> buffer(8);

  REQUIRE_THROWS_AS(s1.raw_recv(buffer), std::system_error);
}

TEST_CASE("Base raw_send and raw_recv transmit exact bytes") {
  int fds[2];
  REQUIRE(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);

  TestSocket s1(SocketDescriptorHandle{fds[0]}, AF,
                SocketFlags::SocketType::Stream,
                SocketFlags::ProtocolType::TCP);

  TestSocket s2(SocketDescriptorHandle{fds[1]}, AF,
                SocketFlags::SocketType::Stream,
                SocketFlags::ProtocolType::TCP);

  std::vector<std::byte> data{std::byte{0x10}, std::byte{0x20}};
  std::vector<std::byte> recv(2);

  auto sent = s1.raw_send(data);
  REQUIRE(sent == 2);

  auto received = s2.raw_recv(recv);
  REQUIRE(received == 2);

  REQUIRE(recv == data);
}

TEST_CASE("Shutdown sending prevents further sends") {
  int fds[2];
  REQUIRE(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);

  TestSocket s1(SocketDescriptorHandle{fds[0]}, AF,
                SocketFlags::SocketType::Stream,
                SocketFlags::ProtocolType::TCP);

  TestSocket s2(SocketDescriptorHandle{fds[1]}, AF,
                SocketFlags::SocketType::Stream,
                SocketFlags::ProtocolType::TCP);

  s1.shutdown(SocketFlags::ShutdownType::Sending);

  std::vector<std::byte> data{std::byte{0x01}};
  REQUIRE_THROWS(s1.raw_send(data));
}

TEST_CASE("FD_CLOEXEC is set when non-inheritable") {
  Socket s(AF, SocketFlags::SocketType::Stream, SocketFlags::ProtocolType::TCP,
           BLOCK, NOINHERIT);

  int flags = fcntl(s.native_handle(), F_GETFD);
  REQUIRE(flags & FD_CLOEXEC);
}
#else
#include <winsock2.h>
#include <ws2tcpip.h>

static std::pair<SOCKET, SOCKET> create_socket_pair() {
  SOCKET listener = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  REQUIRE(listener != INVALID_SOCKET);

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = 0; // let OS choose port

  REQUIRE(::bind(listener, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) ==
          0);
  REQUIRE(::listen(listener, 1) == 0);

  int len = sizeof(addr);
  REQUIRE(::getsockname(listener, reinterpret_cast<sockaddr *>(&addr), &len) ==
          0);

  SOCKET client = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  REQUIRE(client != INVALID_SOCKET);

  REQUIRE(::connect(client, reinterpret_cast<sockaddr *>(&addr),
                    sizeof(addr)) == 0);

  SOCKET server = ::accept(listener, nullptr, nullptr);
  REQUIRE(server != INVALID_SOCKET);

  ::closesocket(listener);

  return {client, server};
}

TEST_CASE("Windows base raw_send and raw_recv work") {
  auto [c, s] = create_socket_pair();

  TestSocket client(SocketDescriptorHandle{c}, SocketFlags::AddressFamily::IPV4,
                    SocketFlags::SocketType::Stream,
                    SocketFlags::ProtocolType::TCP);

  TestSocket server(SocketDescriptorHandle{s}, SocketFlags::AddressFamily::IPV4,
                    SocketFlags::SocketType::Stream,
                    SocketFlags::ProtocolType::TCP);

  std::vector<std::byte> data{std::byte{0xAA}, std::byte{0xBB}};
  std::vector<std::byte> buffer(2);

  REQUIRE(client.raw_send(data) == 2);
  REQUIRE(server.raw_recv(buffer) == 2);
  REQUIRE(buffer == data);
}

TEST_CASE("Windows non-blocking recv returns WSAEWOULDBLOCK") {
  auto [c, s] = create_socket_pair();

  TestSocket server(SocketDescriptorHandle{s}, SocketFlags::AddressFamily::IPV4,
                    SocketFlags::SocketType::Stream,
                    SocketFlags::ProtocolType::TCP,
                    SocketFlags::BlockingType::NonBlocking);

  std::vector<std::byte> buffer(8);

  try {
    auto size = server.raw_recv(buffer);
    FAIL("Expected exception");
  } catch (const std::system_error &e) {
    REQUIRE(e.code().value() == WSAEWOULDBLOCK);
  }

  ::closesocket(c);
}

TEST_CASE("Windows inheritable flag works") {
  auto sock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  REQUIRE(sock != INVALID_SOCKET);

  TestSocket s(SocketDescriptorHandle{sock}, SocketFlags::AddressFamily::IPV4,
               SocketFlags::SocketType::Stream, SocketFlags::ProtocolType::TCP);

  HANDLE h = reinterpret_cast<HANDLE>(sock);

  s.setInheritable(SocketFlags::InheritableType::Inheritable);

  DWORD flags = 0;
  REQUIRE(GetHandleInformation(h, &flags));
  REQUIRE(flags & HANDLE_FLAG_INHERIT);

  s.setInheritable(SocketFlags::InheritableType::NonInheritable);

  REQUIRE(GetHandleInformation(h, &flags));
  REQUIRE((flags & HANDLE_FLAG_INHERIT) == 0);
}

TEST_CASE("Windows shutdown sending prevents further send") {
  auto [c, s] = create_socket_pair();

  TestSocket client(SocketDescriptorHandle{c}, SocketFlags::AddressFamily::IPV4,
                    SocketFlags::SocketType::Stream,
                    SocketFlags::ProtocolType::TCP);

  client.shutdown(SocketFlags::ShutdownType::Sending);

  std::vector<std::byte> data{std::byte{0x01}};

  REQUIRE_THROWS(client.raw_send(data));

  ::closesocket(s);
}

TEST_CASE("Windows close invalidates handle") {
  auto sock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  REQUIRE(sock != INVALID_SOCKET);

  TestSocket s(SocketDescriptorHandle{sock}, SocketFlags::AddressFamily::IPV4,
               SocketFlags::SocketType::Stream, SocketFlags::ProtocolType::TCP);

  s.close();

  REQUIRE(s.native_handle() == INVALID_SOCKET);
}

TEST_CASE("Windows moved-from socket throws") {
  auto sock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  REQUIRE(sock != INVALID_SOCKET);

  TestSocket s1(SocketDescriptorHandle{sock}, SocketFlags::AddressFamily::IPV4,
                SocketFlags::SocketType::Stream,
                SocketFlags::ProtocolType::TCP);

  TestSocket s2(std::move(s1));

  std::vector<std::byte> buffer(4);

  REQUIRE_THROWS(s1.raw_recv(buffer));
}

#endif
