#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

#include "net/connection/iacceptor.h"
#include "net/connection/iconnection.h"
#include "net/coroutine/thread_pool.h"

namespace net::http {

struct StringHash {
  using is_transparent = void; // Enables heterogeneous lookup
  size_t operator()(std::string_view sv) const {
    return std::hash<std::string_view>{}(sv);
  }
  size_t operator()(const std::string &s) const {
    return std::hash<std::string_view>{}(s);
  }
};

class HttpServer {
public:
  using Handler = std::function<std::string(std::string_view)>;

  HttpServer(thread_pool &pool, std::unique_ptr<IAcceptor> acceptor)
      : pool_{pool}, acceptor_{std::move(acceptor)} {}

  void add_route(std::string path, Handler handler) {
    routes_[std::move(path)] = std::move(handler);
  }

  task<void> serve();

  void stop() {
    stop_flag_.store(true, std::memory_order_release);

    acceptor_->close(); // VERY IMPORTANT

    // reactor_.wakeup(); // unblock poll wait
  }

private:
  task<void> handle_client(std::unique_ptr<IConnection> conn);

  std::string build_response(std::string_view body);

  thread_pool &pool_;
  std::unique_ptr<IAcceptor> acceptor_;
  std::map<std::string, Handler, std::less<>> routes_;
  std::atomic<bool> stop_flag_{false};
};

} // namespace net::http
