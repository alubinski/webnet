#pragma once

#include "net/core/endpoint.h"
#include "net/coroutine/spawn.h"
#include "net/coroutine/task.h"
#include "net/coroutine/thread_pool.h"
#include "net/coroutine/wait.h"
#include "net/server/http_worker.h"
#include <atomic>
#include <future>
#include <memory>

namespace net::http {

class HttpServer {
public:
  HttpServer(Endpoint ep) : ep_(ep) {}
  HttpServer(const HttpServer &) = delete;
  HttpServer &operator=(const HttpServer &) = delete;
  // void serve();

  ~HttpServer() { std::cout << "[HttpServer] Destructor called\n"; }

  task<> make_worker(HttpServer *self) {
    std::cout << "[HttpServer] worker coroutine created\n";

    // self is now safely stored in the coroutine frame

    std::cout << "[HttpServer] worker running on thread\n";
    HttpWorker worker;
    worker.init(self->ep_);
    resolved_endpoint_ = worker.local_endpoint();
    co_await self->pool_.schedule();
    epoll_context::get_instance().run();
  }

  void serve() {

    auto worker_pipeline = std::views::iota(0) |
                           std::views::take(pool_.size()) |
                           std::views::transform([this](auto) {
                             // 2. Pass 'this' as an argument, not a capture
                             return make_worker(this);
                           }) |
                           std::ranges::to<std::vector<task<>>>();

    // if (!running_->load())
    //   return;

    std::ranges::for_each(worker_pipeline, spawn<task<> &>);
    std::ranges::for_each(worker_pipeline, wait<task<> &>);
    // for (auto &t : worker_pipeline) {
    //   spawn(std::move(t));
    // }
    // shutdown_promise_.get_future().wait();
    std::cout << "helo\n";
    // stop();
  }

  Endpoint ep_;
  Endpoint resolved_endpoint_;
  thread_pool pool_;

private:
  // std::promise<void> shutdown_promise_;
  // std::shared_ptr<std::atomic<bool>> running_;
};

} // namespace net::http
