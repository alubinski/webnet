#pragma once
#include "net/connection/iacceptor.h"
#include "net/connection/iconnection.h"
#include "net/core/endpoint.h"
#include "net/coroutine/reactor.h"
#include "net/coroutine/task.h"
#include "net/coroutine/thread_pool.h"
#include "net/io/io_context.h"
#include "net/server/http_parser.h"
#include "net/server/http_response.h"
#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <thread>

namespace net::http {

class Worker {
public:
  using Handler = std::function<std::string(std::string_view)>;
  using Routes = std::map<std::string, Handler, std::less<>>;

  explicit Worker(const Routes &routes) : routes_{routes} {}

  Worker(const Worker &) = delete;
  Worker &operator=(const Worker &) = delete;

  void start(const Endpoint &ep);
  void stop();

private:
  task<void> accept_loop(std::shared_ptr<IAcceptor> acceptor);
  task<void> handle_client(std::shared_ptr<IConnection> conn);
  task<void> finalize_connection(std::shared_ptr<IConnection> conn);

  io_context ctx_;
  thread_pool pool_;
  const Routes &routes_;
  std::atomic<bool> stop_flag_{false};
};

} // namespace net::http
