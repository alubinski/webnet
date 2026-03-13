#include "net/server/http_server.h"
#include "net/detail/socket_flags.h"
#include <utility>

namespace net::http {

// template <typename T> void start_detached(task<T> &&t) {
//   auto h = t.release_handle();         // steal handle
//   h.promise().set_detached_flag(true); // ensure detached
//   h.resume();                          // start execution
// }

static std::string parse_path(std::string_view request) {
  // find first line
  auto line_end = request.find("\r\n");
  if (line_end == std::string_view::npos)
    return "/";

  auto first_line = request.substr(0, line_end);

  // format: METHOD SP PATH SP HTTP/VERSION
  auto method_end = first_line.find(' ');
  if (method_end == std::string_view::npos)
    return "/";

  auto path_start = method_end + 1;

  auto path_end = first_line.find(' ', path_start);
  if (path_end == std::string_view::npos)
    return "/";

  return std::string(first_line.substr(path_start, path_end - path_start));
}

task<void> HttpServer::serve() {
  // Move execution to worker thread
  co_await pool_.schedule();

  while (!stop_flag_.load(std::memory_order_acquire)) {

    auto conn = co_await acceptor_->async_accept();

    co_await handle_client(std::move(conn));
    // auto client_task = handle_client(std::move(conn));
    // start_detached(std::move(client_task));
  }
}

task<void> HttpServer::handle_client(std::unique_ptr<IConnection> conn) {

  std::array<std::byte, 4096> buffer{};

  auto n = co_await conn->async_read(buffer);

  if (n <= 0)
    co_return;

  std::string request(reinterpret_cast<char *>(buffer.data()), n);

  auto path = parse_path(request);

  std::string body;

  if (auto it = routes_.find(path); it != routes_.end()) {
    body = it->second(path);
  } else {
    body = "Not Found";
  }

  auto response = build_response(body);

  co_await conn->async_write(std::as_bytes(std::span(response)));

  conn->close();
}

std::string HttpServer::build_response(std::string_view body) {

  std::string resp;

  resp += "HTTP/1.1 200 OK\r\n";
  resp += "Content-Length: " + std::to_string(body.size()) + "\r\n";
  resp += "Connection: close\r\n";
  resp += "\r\n";
  resp += body;

  return resp;
}

} // namespace net::http
