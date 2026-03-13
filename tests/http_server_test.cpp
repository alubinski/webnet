#include <cassert>
#include <chrono>
#include <thread>

#include "catch2/catch_test_macros.hpp"
#include "net/coroutine/reactor.h"
#include "net/detail/socket_flags.h"
#include "net/poll/poll_factory.h"
#include "net/protocol/tcp/tcp_acceptor.h"
#include "net/protocol/tcp/tcp_socket.h"
#include "net/server/http_server.h"

TEST_CASE("http server - basic") {

  // ---------- Setup reactor ----------

  net::thread_pool pool;
  auto poller = net::CreateDefaultReactor();
  net::Reactor reactor(pool, std::move(poller));

  // ---------- Create acceptor (IAcceptor) ----------

  auto acceptor = std::make_unique<net::TcpAcceptor>(reactor);

  acceptor->bind({"127.0.0.1", 8081});
  acceptor->listen();

  // ---------- Create HTTP server ----------

  net::http::HttpServer server(pool, std::move(acceptor));

  server.add_route("/", [](auto) { return "Hello Test"; });

  task(server.serve()).detach();
  reactor.start();

  // ---------- Start reactor thread ----------

  // Give server time to start
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  // ---------- Blocking client ----------

  net::TcpSocket sock(net::detail::SocketFlags::AddressFamily::IPV4,
                      net::detail::SocketFlags::BlockingType::Blocking);

  REQUIRE(sock.is_valid());

  sock.connect({"127.0.0.1", 8081});

  const char *request = "GET / HTTP/1.1\r\n"
                        "Host: localhost\r\n"
                        "\r\n";
  auto request_bytes = std::as_bytes(std::span(request, std::strlen(request)));

  auto send = sock.send(request_bytes);
  REQUIRE(send > 0);

  std::array<std::byte, 4096> buffer{};

  auto n = sock.receive(buffer);
  REQUIRE(n > 0);

  std::string response(reinterpret_cast<char *>(buffer.data()), n);

  std::cout << response << std::endl;

  // ---------- Validate ----------

  REQUIRE(response.find("Hello Test") != std::string::npos);

  sock.close();

  // ---------- Shutdown ----------

  server.stop();

  // Force the server to wake up from `co_await accept()`
  // net::TcpSocket dummy_sock(net::detail::SocketFlags::AddressFamily::IPV4,
  //                           net::detail::SocketFlags::BlockingType::Blocking);
  // dummy_sock.connect({"127.0.0.1", 8081});
  // dummy_sock.close();
  // reactor.stop();
}

TEST_CASE("http server - concurrent requests") {
  // 1. Setup
  net::thread_pool pool(4); // Use 4 threads for concurrency
  auto poller = net::CreateDefaultReactor();
  net::Reactor reactor(pool, std::move(poller));
  auto acceptor = std::make_unique<net::TcpAcceptor>(reactor);
  acceptor->bind({"127.0.0.1", 8082});
  acceptor->listen();
  net::http::HttpServer server(pool, std::move(acceptor));
  server.add_route("/test", [](auto) { return "Concurrent"; });

  (server.serve()).detach();
  std::thread reactor_thread([&]() { reactor.start(); });
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  // 2. Fire multiple clients
  const int client_count = 10;
  std::vector<std::thread> clients;
  std::atomic<int> success_count{0};

  for (int i = 0; i < client_count; ++i) {
    clients.emplace_back([&success_count]() {
      net::TcpSocket sock(net::detail::SocketFlags::AddressFamily::IPV4,
                          net::detail::SocketFlags::BlockingType::Blocking);
      sock.connect({"127.0.0.1", 8082});
      const char *request = "GET /test HTTP/1.1\r\nHost: localhost\r\n\r\n";
      sock.send(std::as_bytes(std::span(request, std::strlen(request))));

      std::array<std::byte, 1024> buffer{};
      if (sock.receive(buffer) > 0) {
        success_count++;
      }
    });
  }

  for (auto &t : clients)
    t.join();

  // 3. Validate
  REQUIRE(success_count == client_count);

  // 4. Teardown
  server.stop();
  reactor.stop();
  reactor_thread.join();
}
