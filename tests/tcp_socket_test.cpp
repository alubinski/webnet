#include "catch2/catch_test_macros.hpp"
#include "net/core/endpoint.h"
#include "net/detail/socket_flags.h"
#include "net/protocol/tcp/tcp_socket.h"
#include <catch2/catch_all.hpp>
#include <future>
#include <iostream>
#include <thread>

TEST_CASE("TcpSocket basic connect/accept", "[tcp]") {
  using namespace net;

  TcpSocket server(detail::SocketFlags::AddressFamily::IPV4,
                   detail::SocketFlags::BlockingType::Blocking);

  Endpoint ep("127.0.0.1", 0);
  server.setReuseAddress(true);
  server.bind(ep);
  server.listen();

  Endpoint bound = server.localEndpoint();

  std::promise<void> ready;
  std::future<void> ready_fut = ready.get_future();
  std::exception_ptr eptr;

  std::thread server_thread([server = std::ref(server), &ready, &eptr] {
    try {
      Endpoint peer;
      ready.set_value();
      TcpSocket client = server.get().accept(peer);
      REQUIRE(client.is_valid());
    } catch (...) {
      eptr = std::current_exception();
    }
  });

  ready_fut.wait();

  TcpSocket client(detail::SocketFlags::AddressFamily::IPV4,
                   detail::SocketFlags::BlockingType::Blocking);

  client.connect(bound);

  server_thread.join();

  if (eptr)
    std::rethrow_exception(eptr);
}

TEST_CASE("TcpSocket send & recv", "[tcp]") {
  using namespace net;

  TcpSocket server(detail::SocketFlags::AddressFamily::IPV4,
                   detail::SocketFlags::BlockingType::Blocking);

  Endpoint ep("127.0.0.1", 0);

  server.setReuseAddress(true);
  server.bind(ep);
  server.listen();

  Endpoint bound = server.localEndpoint();

  std::thread server_thread([&] {
    Endpoint peer;
    TcpSocket conn = server.accept(peer);

    REQUIRE(conn.is_valid());

    // ---- RECEIVE "hello" ----
    std::array<std::byte, 5> msg{};
    auto view = std::span(msg);

    std::size_t total = 0;
    while (total < view.size()) {
      auto n = conn.receive(view.subspan(total));
      REQUIRE(n >= 0);
      total += n;
    }

    REQUIRE(std::string_view(reinterpret_cast<const char *>(msg.data()),
                             msg.size()) == "hello");

    // ---- SEND "world" ----
    std::array<std::byte, 5> reply{std::byte{'w'}, std::byte{'o'},
                                   std::byte{'r'}, std::byte{'l'},
                                   std::byte{'d'}};

    auto reply_view = std::span(reply);

    std::size_t sent_total = 0;
    while (sent_total < reply_view.size()) {
      auto n = conn.send(reply_view.subspan(sent_total));
      REQUIRE(n >= 0);
      sent_total += n;
    }
  });

  // ---- CLIENT SIDE ----
  TcpSocket client(detail::SocketFlags::AddressFamily::IPV4,
                   detail::SocketFlags::BlockingType::Blocking);

  client.connect(bound);

  // ---- SEND "hello" ----
  std::array<std::byte, 5> hello{std::byte{'h'}, std::byte{'e'}, std::byte{'l'},
                                 std::byte{'l'}, std::byte{'o'}};

  auto hello_view = std::span(hello);

  std::size_t sent_total = 0;
  while (sent_total < hello_view.size()) {
    auto n = client.send(hello_view.subspan(sent_total));
    REQUIRE(n >= 0);
    sent_total += n;
  }

  // ---- RECEIVE "world" ----
  std::array<std::byte, 5> response{};
  auto response_view = std::span(response);

  std::size_t recv_total = 0;
  while (recv_total < response_view.size()) {
    auto n = client.receive(response_view.subspan(recv_total));
    REQUIRE(n >= 0);
    recv_total += n;
  }

  REQUIRE(std::string_view(reinterpret_cast<const char *>(response.data()),
                           response.size()) == "world");

  server_thread.join();
}

TEST_CASE("TcpSocket shutdown produces EOF", "[tcp]") {

  using namespace net;
  TcpSocket server(detail::SocketFlags::AddressFamily::IPV4,
                   detail::SocketFlags::BlockingType::Blocking);

  Endpoint ep("127.0.0.1", 0);
  server.setReuseAddress(true);
  server.bind(ep);
  server.listen();

  Endpoint bound = server.localEndpoint();

  std::thread server_thread([&] {
    Endpoint peer;
    TcpSocket conn = server.accept(peer);

    std::array<std::byte, 8> buffer{};

    std::size_t n = conn.receive(buffer);

    REQUIRE(n == 0);
  });

  TcpSocket client(net::detail::SocketFlags::AddressFamily::IPV4,
                   net::detail::SocketFlags::BlockingType::Blocking);

  client.connect(bound);

  client.shutdown(detail::SocketFlags::ShutdownType::Sending);

  server_thread.join();
}

TEST_CASE("TcpSocket move transfers ownership", "[tcp]") {
  using namespace net;

  TcpSocket a(net::detail::SocketFlags::AddressFamily::IPV4,
              net::detail::SocketFlags::BlockingType::Blocking);

  REQUIRE(a.is_valid());

  TcpSocket b = std::move(a);

  REQUIRE(b.is_valid());
  REQUIRE_FALSE(a.is_valid());
}

TEST_CASE("TcpSocket invalid operations throw", "[tcp]") {
  using namespace net;

  TcpSocket sock;

  std::array<std::byte, 8> buf{};

  REQUIRE_THROWS(sock.send(buf));
  REQUIRE_THROWS(sock.receive(buf));
  REQUIRE_THROWS(sock.shutdown(net::detail::SocketFlags::ShutdownType::Both));
}
