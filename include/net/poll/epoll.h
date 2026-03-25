#include "net/poll/ipoll.h"
#include <array>
#include <stdexcept>
#include <sys/epoll.h>
#include <system_error>
#include <unistd.h>
#include <vector>

namespace net {

class EpollPoll : public detail::IPoll {
public:
  EpollPoll() {
    epoll_fd_ = ::epoll_create1(0);
    if (epoll_fd_ == -1) {
      throw std::system_error(errno, std::generic_category(),
                              "epoll_create1 failed");
    }
  }

  ~EpollPoll() override {
    if (epoll_fd_ != -1)
      ::close(epoll_fd_);
  }

  void add(fd_t fd, PollEvent events) override {
    struct epoll_event ev;
    ev.events = to_native(events) | EPOLLET; // Edge-Triggered mode
    ev.data.fd = fd;
    if (::epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) == -1) {
      throw std::system_error(errno, std::generic_category(),
                              "epoll_ctl ADD failed");
    }
  }

  void modify(fd_t fd, PollEvent events) override {
    struct epoll_event ev;
    ev.events = to_native(events) | EPOLLET;
    ev.data.fd = fd;
    if (::epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev) == -1) {
      throw std::system_error(errno, std::generic_category(),
                              "epoll_ctl MOD failed");
    }
  }

  void remove(fd_t fd) override {
    ::epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
  }

  void cancel(fd_t fd) override {
    // EPOLL_CTL_DEL removes the file descriptor from the interest list.
    // The event parameter is ignored for DEL, so we pass nullptr.
    if (::epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr) == -1) {
      // We often ignore ENOENT here because it means the FD was already
      // removed or never added, which is safe for a 'cancel' operation.
      if (errno != ENOENT) {
        // Log or throw if it's a real system error (e.g., EBADF)
      }
    }
  }

  std::vector<PollResult> wait(int timeout_ms) override {
    // Pre-allocate the event buffer on the stack for maximum performance
    std::array<struct epoll_event, 256> events;

    int nfds =
        ::epoll_wait(epoll_fd_, events.data(), events.size(), timeout_ms);

    if (nfds == -1) {
      if (errno == EINTR)
        return {}; // Interrupted by signal, just retry later
      throw std::system_error(errno, std::generic_category(),
                              "epoll_wait failed");
    }

    std::vector<PollResult> results;
    results.reserve(nfds);
    for (int i = 0; i < nfds; ++i) {
      results.push_back(
          {.fd = events[i].data.fd, .events = from_native(events[i].events)});
    }
    return results;
  }

private:
  int epoll_fd_;

  static uint32_t to_native(PollEvent e) {
    uint32_t mask = 0;
    if ((e & PollEvent::Read) == PollEvent::Read)
      mask |= EPOLLIN;
    if ((e & PollEvent::Write) == PollEvent::Write)
      mask |= EPOLLOUT;
    if ((e & PollEvent::Error) == PollEvent::Error)
      mask |= (EPOLLERR | EPOLLHUP);
    return mask;
  }

  static PollEvent from_native(uint32_t events) {
    PollEvent mask = PollEvent::None;
    if (events & EPOLLIN)
      mask |= PollEvent::Read;
    if (events & EPOLLOUT)
      mask |= PollEvent::Write;
    if (events & (EPOLLERR | EPOLLHUP))
      mask |= PollEvent::Error;
    return mask;
  }
};
} // namespace net
