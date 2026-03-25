#include "net/server/worker.h"
#include "net/detail/socket_flags.h"
#include "net/protocol/tcp/tcp_acceptor.h"
#include "net/protocol/tcp/tcp_socket.h"
#include <array>
#include <format>
#include <span>
#include <sys/socket.h>

namespace net::http {

void Worker::start(const Endpoint &ep) {
  TcpSocket sock;
  sock.setReuseAddress(true);

  int opt = 1;
  ::setsockopt(sock.native_handle(), SOL_SOCKET, SO_REUSEPORT, &opt,
               sizeof(opt));

  sock.bind(ep);
  sock.listen();

  auto acceptor = std::make_shared<TcpAcceptor>(std::move(sock));

  pool_.detach(accept_loop(std::move(acceptor)));
}

void Worker::stop() { stop_flag_.store(true, std::memory_order_release); }

task<void> Worker::accept_loop(std::shared_ptr<IAcceptor> acceptor) {
  while (!stop_flag_.load(std::memory_order_acquire)) {
    try {
      auto connections = co_await acceptor->async_multi_accept();
      for (auto &conn : connections) {
        pool_.detach(handle_client(std::move(conn)));
      }
    } catch (const std::exception &) {
    }
  }
}

task<void> Worker::handle_client(std::shared_ptr<IConnection> conn) {
  co_await pool_.schedule();

  std::string request_data;
  std::array<char, 4096> buffer{};

  while (request_data.find("\r\n\r\n") == std::string::npos) {
    auto n =
        co_await conn->async_read(std::as_writable_bytes(std::span(buffer)));
    if (n <= 0)
      co_return;
    request_data.append(buffer.data(), n);
  }

  auto req_result = parse_http(request_data);
  HttpResponse res{.version = "HTTP/1.1"};

  if (!req_result) {
    res.status = HttpStatus::BadRequest;
    res.body = "Invalid Request";
  } else {
    const auto &req = *req_result;
    if (auto it = routes_.find(req.uri); it != routes_.end()) {
      res.status = HttpStatus::OK;
      res.body = it->second(req.uri);
    } else {
      res.status = HttpStatus::NotFound;
      res.body = "Not Found";
    }
    res.headers.push_back({"Content-Type", "text/plain"});
    res.headers.push_back({"Content-Length", std::to_string(res.body.size())});
  }

  res.headers.push_back({"Connection", "close"});

  std::string wire_data = std::format("{}", res);
  co_await conn->async_write(std::as_bytes(std::span(wire_data)));

  co_await finalize_connection(conn);
}

task<void> Worker::finalize_connection(std::shared_ptr<IConnection> conn) {
  ::shutdown(conn->native_handle(), SHUT_WR);
  try {
    std::array<char, 1024> drain_buf;
    while (true) {
      auto n = co_await conn->async_read(
          std::as_writable_bytes(std::span(drain_buf)));
      if (n <= 0)
        break;
    }
  } catch (...) {
  }
  co_return;
}

} // namespace net::http
