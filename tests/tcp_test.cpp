#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <future>
#include <iostream>
#include <string_view>
#include <thread>
#include <vector>

#include "net/core/endpoint.h"
#include "net/detail/socket_flags.h"
#include "net/poll/poll_factory.h"
#include "net/protocol/tcp/tcp_acceptor.h"
#include "net/protocol/tcp/tcp_connection.h"
#include "net/protocol/tcp/tcp_socket.h"

// TEST_CASE("TcpAcceptor async_accept accepts a client", "[tcp][accept]") {
//   using namespace net;
//
//   auto poller = CreateDefaultReactor();
//   Reactor reactor(std::move(poller));
//
//   TcpAcceptor acceptor(reactor, TcpSocket::AddressFamily::IPV4);
//
//   Endpoint ep{"127.0.0.1", 0};
//   acceptor.bind(ep);
//   acceptor.listen();
//
//   const auto port = acceptor.local_endpoint().port();
//
//   std::atomic<bool> done = false;
//
//   std::thread server_thread([&]() {
//     auto accept_task = acceptor.async_accept();
//
//     while (!accept_task.is_ready()) {
//       reactor.run_once(100);
//     }
//
//     auto conn = accept_task.get();
//
//     REQUIRE(conn != nullptr);
//     REQUIRE(conn->remote_endpoint().port() != 0);
//
//     done = true;
//   });
//
//   // Client
//   TcpSocket client(TcpSocket::AddressFamily::IPV4,
//                    detail::SocketFlags::BlockingType::Blocking);
//
//   client.connect(Endpoint{"127.0.0.1", port});
//
//   while (!done) {
//     std::this_thread::sleep_for(std::chrono::milliseconds(10));
//   }
//
//   server_thread.join();
// }
//
// TEST_CASE("TcpConnection async write/read test", "[tcp][async]") {
//   using namespace net;
//
//   auto poller = CreateDefaultReactor();
//   Reactor reactor(std::move(poller));
//
//   TcpAcceptor acceptor(reactor, TcpSocket::AddressFamily::IPV4);
//
//   Endpoint ep{"127.0.0.1", 0};
//   acceptor.bind(ep);
//   acceptor.listen();
//
//   auto port = acceptor.local_endpoint().port();
//
//   std::atomic<bool> server_done = false;
//
//   std::thread server_thread([&]() {
//     auto accept_task = acceptor.async_accept();
//
//     while (!accept_task.is_ready()) {
//       reactor.run_once(100);
//     }
//
//     auto conn = accept_task.get();
//
//     std::vector<std::byte> buffer(64);
//
//     auto read_task = conn->async_read(buffer);
//
//     while (!read_task.is_ready()) {
//       reactor.run_once(100);
//     }
//
//     auto n = read_task.get();
//
//     std::span data(buffer.data(), n);
//
//     auto write_task = conn->async_write(data);
//
//     while (!write_task.is_ready()) {
//       reactor.run_once(100);
//     }
//
//     write_task.get();
//
//     server_done = true;
//   });
//
//   // Client
//   TcpSocket client(TcpSocket::AddressFamily::IPV4,
//                    detail::SocketFlags::BlockingType::Blocking);
//
//   client.connect(Endpoint{"127.0.0.1", port});
//
//   std::string msg = "hello_async";
//
//   client.send({reinterpret_cast<const std::byte *>(msg.data()), msg.size()});
//
//   std::vector<char> recv_buf(64);
//
//   auto n = client.receive(
//       {reinterpret_cast<std::byte *>(recv_buf.data()), recv_buf.size()});
//
//   REQUIRE(std::string_view(recv_buf.data(), n) == "hello_async");
//
//   while (!server_done) {
//     std::this_thread::sleep_for(std::chrono::milliseconds(10));
//   }
//
//   server_thread.join();
// }
