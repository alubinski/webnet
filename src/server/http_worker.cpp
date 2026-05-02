#include "net/server/http_worker.h"
#include "net/server/http_parser.h"
#include "net/server/http_response.h"
#include <iostream>
#include <memory>
#include <span>
#include <string_view>

namespace net::http {

task<void> HttpWorker::handle_client(std::shared_ptr<IConnection> conn) {
  auto fd = conn->native_handle();
  // std::cout << "[Client " << fd << "] Connection accepted" << std::endl;

  std::string request_data;
  std::array<char, 4096> buffer{};

  while (true) {
    // std::cout << "work2\n";
    auto n =
        co_await conn->async_read(std::as_writable_bytes(std::span(buffer)));
    if (n <= 0) {
      // std::cout << "[Client " << fd << "] Read returned " << n << "(Closing)
      // "
      //           << std::endl;
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
    // std::cout << "[Client " << fd << "] Request received: " << req.uri
    //           << std::endl;

    res.status = HttpStatus::OK;
    res.body = "OK";

    std::string content_len = std::to_string(res.body.size());
    res.headers.push_back({"Content-Type", "text/plain"});
    res.headers.push_back({"Content-Length", content_len});
  }

  std::string wire_data = std::format("{}", res);

  co_await conn->async_write(std::as_bytes(std::span(wire_data)));
  // std::cout << "[Client " << fd << "] Write complete" << std::endl;

  conn->close();
}

} // namespace net::http
