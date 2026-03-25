#include <atomic>
#include <csignal>
#include <iostream>

#include "net/coroutine/reactor.h"
#include "net/io/io_context.h"
#include "net/poll/poll_factory.h"
#include "net/protocol/tcp/tcp_acceptor.h"
#include "net/server/http_server.h"
#include <atomic>
#include <csignal>
#include <memory>

// Global pointer for signal handling (typical in simple C++ servers)
using std::signal;

net::http::HttpServer *global_server = nullptr;

std::atomic_flag shutdown_requested = ATOMIC_FLAG_INIT;

void signal_handler(int sig) {
  if (sig == SIGINT || sig == SIGTERM) {
    shutdown_requested.test_and_set();

    if (global_server) {
      global_server->stop();
    }
  }
}

int main() {
  unsigned int threads = std::thread::hardware_concurrency();
  net::thread_pool pool(threads);
  auto poller = net::CreateDefaultReactor();
  net::Reactor reactor(pool, std::move(poller));

  std::signal(SIGINT, signal_handler);
  std::signal(SIGTERM, signal_handler);

  auto acceptor = std::make_shared<net::TcpAcceptor>(reactor);
  acceptor->bind({"0.0.0.0", 8080}); // Listen on all interfaces
  acceptor->listen();

  // 4. Initialize HTTP Server
  net::http::HttpServer server(pool, std::move(acceptor));
  global_server = &server;

  // Route 1: Simple hello
  server.add_route("/", [](std::string_view) -> std::string {
    return "Welcome to the C++23 Server!";
  });

  // Route 2: Echo some data
  server.add_route("/status", [](std::string_view) -> std::string {
    return "Server is UP and using std::formatter";
  });

  std::cout << "[Server] Thread Pool Size: " << threads << std::endl;
  std::cout << "[Server] Listening on http://0.0.0.0:8080" << std::endl;

  // Spawn the background listener coroutine
  reactor.start();
  task(server.serve()).detach();
  while (true) {
  }

  return 0;
}
