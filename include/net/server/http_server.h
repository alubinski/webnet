#pragma once
#include "net/core/endpoint.h"
#include "net/server/worker.h"
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace net::http {

class HttpServer {
public:
  using Handler = Worker::Handler;

  void add_route(std::string path, Handler handler) {
    routes_[std::move(path)] = std::move(handler);
  }

  void run(const Endpoint &ep,
           std::size_t num_workers = std::thread::hardware_concurrency());

  void stop();

private:
  Worker::Routes routes_;
  std::vector<std::unique_ptr<Worker>> worker_list_;
};

} // namespace net::http