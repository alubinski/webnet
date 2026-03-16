#include "net/server/http_server.h"
#include "net/detail/socket_flags.h"
#include "net/server/http_parser.h"
#include "net/server/http_response.h"
#include <iostream>
#include <span>
#include <string_view>
#include <utility>

namespace net::http {

task<void> HttpServer::serve() {
  // Move execution to worker thread
  co_await pool_.schedule();

  while (!stop_flag_.load(std::memory_order_acquire)) {

    auto conn = co_await acceptor_->async_accept();

    // task(handle_client(std::move(conn))).detach();
    auto client_task = handle_client(std::move(conn));
    client_task.detach();
    // start_detached(std::move(client_task));
  }
}

task<void> HttpServer::handle_client(std::unique_ptr<IConnection> conn) {
  auto fd = conn->native_handle(); // Assuming you have access
  std::cout << "[Client " << fd << "] Connection accepted" << std::endl;

  std::string request_data;
  std::array<char, 4096> buffer{};

  while (true) {
    auto n =
        co_await conn->async_read(std::as_writable_bytes(std::span(buffer)));
    if (n <= 0) {
      std::cout << "[Client " << fd << "] Read returned " << n << " (Closing)"
                << std::endl;
      break;
    }

    request_data.append(buffer.data(), n);
    if (request_data.find("\r\n\r\n") != std::string::npos) {
      break;
    }
  }

  auto req_result = parse_http(request_data);

  HttpResponse res{.version = "HTTP/1.1"};

  if (!req_result) {
    res.status = HttpStatus::BadRequest;
    res.body = "Invalid Request";
  } else {
    const auto &req = *req_result;
    std::cout << "[Client " << fd << "] Request received: " << req.uri
              << std::endl;

    if (auto it = routes_.find(req.uri); it != routes_.end()) {
      res.status = HttpStatus::OK;
      res.body = it->second(req.uri);
    } else {
      res.status = HttpStatus::NotFound;
      res.body = "Not Found";
    }

    std::string content_len = std::to_string(res.body.size());
    res.headers.push_back({"Content-Type", "text/plain"});
    res.headers.push_back({"Content-Length", content_len});
  }

  std::string wire_data = std::format("{}", res);

  co_await conn->async_write(std::as_bytes(std::span(wire_data)));
  std::cout << "[Client " << fd << "] Write complete" << std::endl;

  conn->close();
}

} // namespace net::http
