#pragma once

#include "net/poll/ipoll.h"
#include <array>
#include <atomic>
#include <coroutine>
#include <cstring> // For strerror
#include <iostream>
#include <memory>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <vector>

namespace net {

class epoll_context {
public:
  static auto get_instance() -> epoll_context & {
    static thread_local epoll_context instance;
    // std::clog << "[epoll_context] Initialized on Thread: " << pthread_self()
    //           << " | epoll_fd: " << instance.epoll_fd_ << "\n";
    return instance;
  }

  epoll_context() {
    epoll_fd_ = ::epoll_create1(EPOLL_CLOEXEC);
    // if (epoll_fd_ == -1) {
    //   // std::cerr << "[epoll_context] CRITICAL: Failed to create epoll fd: "
    //           << std::strerror(errno) << "\n";
    // } else {
    //   std::clog << "[epoll_context] Initialized. epoll_fd: " << epoll_fd_
    //             << "\n";
    // }
    // Note: 65535 is a common soft limit, but ensure your system RLIMIT_NOFILE
    // supports this.
    stop_fd_ = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);

    epoll_event ev{.events = EPOLLIN, .data = {.fd = stop_fd_}};
    ::epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, stop_fd_, &ev);
    continuations_.resize(65535, nullptr);
  }

  ~epoll_context() {
    if (stop_fd_ > 0)
      ::close(stop_fd_);
    if (epoll_fd_ > 0) {
      // std::clog << "[epoll_context] Closing epoll_fd: " << epoll_fd_ << "\n";
      ::close(epoll_fd_);
    }
  }

  // Maps an FD to a coroutine and arms epoll
  void watch(fd_t fd, uint32_t events, std::coroutine_handle<> h) {
    if (fd < 0 || static_cast<size_t>(fd) >= continuations_.size()) {
      // std::cerr << "[epoll_context] ERROR: fd " << fd << " out of bounds\n";
      return;
    }

    // std::clog << "[epoll_context] Watching fd: " << fd
    //           << " | Events: " << events << " | Coro: " << h.address() <<
    //           "\n";

    continuations_[fd] = h;
    epoll_event ev{.events = events | EPOLLET, .data = {.fd = fd}};

    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) == -1) {
    }
    // Try to modify, if not exists (ENOENT), add it
    // if (epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev) == -1) {
    //   if (errno == ENOENT) {
    //     if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) == -1) {
    //       // std::cerr << "[epoll_context] ERROR: EPOLL_CTL_ADD failed for fd
    //       "
    //       //           << fd << ": " << std::strerror(errno) << "\n";
    //     }
    //   } else {
    //     // std::cerr << "[epoll_context] ERROR: EPOLL_CTL_MOD failed for fd "
    //     <<
    //     // fd
    //     //           << ": " << std::strerror(errno) << "\n";
    //   }
    // }
  }

  void unwatch(int fd) noexcept {
    if (fd < 0 || static_cast<size_t>(fd) >= continuations_.size())
      return;

    // std::clog << "[epoll_context] Unwatching fd: " << fd << "\n";
    continuations_[fd] = nullptr;

    if (epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr) == -1) {
      // ENOENT is common if the FD was already closed or never added
      if (errno != ENOENT) {
        // std::cerr << "[epoll_context] WARNING: EPOLL_CTL_DEL failed for fd "
        //           << fd << ": " << std::strerror(errno) << "\n";
      }
    }
  }

  void run() {
    // std::clog << "[epoll_context] Entering event loop...\n";
    std::vector<epoll_event> events(128);

    while (true) {
      int n = epoll_wait(epoll_fd_, events.data(),
                         static_cast<int>(events.size()), 100);
      if (n == 0)
        continue;

      if (n < 0) {
        if (errno == EINTR)
          continue; // Interrupted by signal
        // std::cerr << "[epoll_context] epoll_wait error: "
        //           << std::strerror(errno) << "\n";
        break;
      }

      for (int i = 0; i < n; ++i) {
        int fd = events[i].data.fd;

        if (fd == stop_fd_) {
          // Just consuming the eventfd value is enough to wake the loop
          uint64_t dummy;
          [[maybe_unused]] auto res = ::read(stop_fd_, &dummy, sizeof(dummy));
          continue;
        }

        auto h = continuations_[fd];

        if (h) {
          // std::clog << "[epoll_context] Event on fd: " << fd
          //           << " | Resuming coro: " << h.address() << "\n";

          // Reset mapping before resuming (EPOLLONESHOT behavior)
          continuations_[fd] = nullptr;
          h.resume();
        } else {
          // std::cerr << "[epoll_context] WARNING: Received event for fd " <<
          // fd
          //           << " but no coroutine is registered.\n";
        }
      }
    }
    std::clog << "[epoll_context] Loop exited.\n";
  }

  void stop() {
    std::clog << "[epoll_context] Stop requested.\n";
    uint64_t val = 1;
    // Writing to eventfd makes epoll_wait return immediately
    ::write(stop_fd_, &val, sizeof(val));
  }

private:
  fd_t epoll_fd_;
  fd_t stop_fd_;
  std::vector<std::coroutine_handle<>> continuations_;
};

} // namespace net
