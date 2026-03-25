#include "net/server/http_server.h"
#include <algorithm>
#include <ranges>

namespace net::http {

void HttpServer::run(const Endpoint &ep, std::size_t num_workers) {
  worker_list_ = std::views::iota(0u, num_workers)
                 | std::views::transform([&](auto) {
                     return std::make_unique<Worker>(routes_);
                   })
                 | std::ranges::to<std::vector>();

  std::ranges::for_each(worker_list_, [&ep](auto &w) { w->start(ep); });
}

void HttpServer::stop() {
  std::ranges::for_each(worker_list_, [](auto &w) { w->stop(); });
}

} // namespace net::http