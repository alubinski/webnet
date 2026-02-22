#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <future>
#include <iostream>
#include <string_view>
#include <thread>
#include <vector>

#include "net/core/endpoint.h"
#include "net/detail/socket_flags.h"
#include "net/protocol/tcp/tcp_acceptor.h"
#include "net/protocol/tcp/tcp_connection.h"
#include "net/protocol/tcp/tcp_socket.h"

template <typename T> auto wait_task(net::task<T> &t) { return t.get(); }

TEST_CASE("TcpAcceptor async_accept accepts a client", "[tcp][accept]") {
  using namespace net;

  TcpAcceptor acceptor(TcpSocket::AddressFamily::IPV4);

  Endpoint ep{"127.0.0.1", 0};
  acceptor.bind(ep);
  acceptor.listen();

  const auto port = acceptor.local_endpoint().port();

  // Barrier: client connected
  std::promise<void> client_connected;
  auto client_connected_future = client_connected.get_future();

  std::jthread server_thread([&]() {
    // Start accept coroutine
    auto accept_task = acceptor.async_accept();

    // Wait until client actually connects
    client_connected_future.get();

    // Now it's safe to resume accept
    acceptor.notify_readable();

    auto conn = accept_task.get();

    REQUIRE(conn != nullptr);
    REQUIRE(conn->remote_endpoint().port() != 0);
  });

  // --- Client side (blocking) ---
  TcpSocket client(TcpSocket::AddressFamily::IPV4,
                   detail::SocketFlags::BlockingType::Blocking);

  client.connect(Endpoint{"127.0.0.1", port});

  // Signal server that connection exists
  client_connected.set_value();
}

TEST_CASE("TcpConnection async write/read test", "[tcp][async]") {
  using namespace net;

  TcpAcceptor acceptor(TcpSocket::AddressFamily::IPV4);

  Endpoint ep{"127.0.0.1", 0};
  acceptor.bind(ep);
  acceptor.listen();

  auto port = acceptor.local_endpoint().port();

  std::promise<void> client_sent;
  std::promise<void> server_ready;

  auto client_sent_future = client_sent.get_future();
  auto server_ready_future = server_ready.get_future();

  std::jthread server_thread([&]() {
    try {
      server_ready.set_value();

      auto accept_task = acceptor.async_accept();
      acceptor.notify_readable();

      auto conn = accept_task.get();

      std::vector<std::byte> buffer(64);

      auto read_task = conn->async_read(buffer);

      client_sent_future.get();
      conn->notify_readable();

      auto n = read_task.get();

      std::span data(buffer.data(), n);

      auto write_task = conn->async_write(data);
      conn->notify_writable();

      write_task.get();
    } catch (const std::exception &e) {
      std::cerr << "SERVER THREAD ERROR: " << e.what() << "\n";
      std::terminate();
    }
  });

  server_ready_future.get();

  // Client side blocking socket (test determinism)
  TcpSocket client(TcpSocket::AddressFamily::IPV4,
                   detail::SocketFlags::BlockingType::Blocking);

  client.connect(Endpoint{"127.0.0.1", port});

  std::string msg = "hello_async";

  client.send({reinterpret_cast<const std::byte *>(msg.data()), msg.size()});

  client_sent.set_value();

  std::vector<char> recv_buf(64);

  auto n = client.receive(
      {reinterpret_cast<std::byte *>(recv_buf.data()), recv_buf.size()});

  REQUIRE(std::string_view(recv_buf.data(), n) == "hello_async");
}
