#pragma once
#include "net/coroutine/task.h"
#include "net/poll/ipoll.h"
#include "net/poll/poll_factory.h"
#include <coroutine>
#include <cstddef>
#include <deque>
#include <memory>
#include <vector>

namespace net {

class io_context {
public:
  explicit io_context(std::size_t max_fds = 65535) {
    continuations_.resize(max_fds, nullptr);
    in_epoll_.resize(max_fds, false);
  }

  io_context(const io_context &) = delete;
  io_context &operator=(const io_context &) = delete;

  // First registration or direction change (Read↔Write): EPOLL_CTL_ADD or MOD
  void watch(int fd, PollEvent events, std::coroutine_handle<> h) noexcept {
    set_continuation(fd, h);
    if (!in_epoll_[fd]) {
      poller_->add(fd, events);
      in_epoll_[fd] = true;
    } else {
      poller_->modify(fd, events);
    }
  }

  // ET mode: fd already registered, just refresh the wakeup handle (no syscall)
  void arm(int fd, std::coroutine_handle<> h) noexcept {
    set_continuation(fd, h);
  }

  // Remove fd from poller and clear its continuation
  void unwatch(int fd) noexcept {
    if (static_cast<std::size_t>(fd) < continuations_.size()) {
      continuations_[fd] = nullptr;
      in_epoll_[fd] = false;
    }
    poller_->remove(fd);
  }

  // Detach a task<void> and schedule it to start on the next run() tick
  void spawn(task<void> &&t) {
    auto h = t.take_handle();
    if (h) {
      h.promise().detached = true;
      pending_.push_back(h);
    }
  }

  // Queue a raw coroutine handle to run on the next run() tick
  void post(std::coroutine_handle<> h) { pending_.push_back(h); }

  void stop() noexcept { running_ = false; }

  [[nodiscard]] detail::IPoll &get_poller() noexcept { return *poller_; }

  void run() {
    running_ = true;

    while (running_) {
      while (!pending_.empty()) {
        auto h = pending_.front();
        pending_.pop_front();
        if (h && !h.done())
          h.resume();
      }

      const int timeout_ms = pending_.empty() ? -1 : 0;
      auto events = poller_->wait(timeout_ms);

      for (const auto &ev : events) {
        resume(ev.fd);
      }
    }
  }

private:
  void set_continuation(int fd, std::coroutine_handle<> h) noexcept {
    if (static_cast<std::size_t>(fd) < continuations_.size()) {
      continuations_[fd] = h;
    } else {
      std::terminate();
    }
  }

  void resume(int fd) noexcept {
    if (static_cast<std::size_t>(fd) >= continuations_.size())
      return;
    auto h = continuations_[fd];
    if (h && !h.done()) {
      continuations_[fd] = nullptr;
      h.resume();
    }
  }

  std::vector<std::coroutine_handle<>> continuations_;
  std::vector<bool> in_epoll_;
  std::deque<std::coroutine_handle<>> pending_;
  std::unique_ptr<detail::IPoll> poller_{CreateDefaultReactor()};
  bool running_{false};
};

} // namespace net
