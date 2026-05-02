#include "net/server/http_server.h"
#include "net/core/endpoint.h"
#include "net/coroutine/spawn.h"
#include "net/coroutine/task.h"
#include "net/coroutine/thread_pool.h"
#include "net/coroutine/wait.h"
#include "net/detail/socket_flags.h"
#include "net/poll/epoll_context.h"
#include "net/server/http_parser.h"
#include "net/server/http_response.h"
#include "net/server/http_worker.h"
#include <algorithm>
#include <chrono>
#include <future>
#include <iostream>
#include <ranges>
#include <span>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace net::http {

// void HttpServer::serve() {
//   auto worker_pipeline =
//       std::views::iota(0) | std::views::take(pool_.size()) |
//       std::views::transform([this](auto) noexcept -> task<> {
//         return [this]() noexcept -> task<> {
//           std::cout << "[HttpServer] worker coroutine created\n";
//           co_await pool_.schedule();
//
//           std::cout << "[HttpServer] worker running on thread\n";
//           HttpWorker worker;
//           worker.init(ep_);
//           epoll_context::get_instance().run();
//         }();
//       }) |
//       std::ranges::to<std::vector<task<>>>();
//
//   std::ranges::for_each(worker_pipeline, spawn<task<> &>);
//   std::ranges::for_each(worker_pipeline, wait<task<> &>);
//   // std::ranges::for_each(worker_pipeline, [](task<> &t) { t.detach(); });
//   // shutdown_promise_.get_future().wait();
// }

} // namespace net::http
