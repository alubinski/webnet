#include "net/poll/ipoll.h"
#include <mutex>
#include <unordered_map>
#include <winsock2.h>

namespace net {

class WindowsPoll : public detail::IPoll {
public:
  void add(fd_t fd, PollEvent events) override {
    std::lock_guard lock(mutex_);

    fd_set_map_[fd] = events;
  }

  void modify(fd_t fd, PollEvent events) override { add(fd, events); }

  void remove(fd_t fd) override {
    std::lock_guard lock(mutex_);
    fd_set_map_.erase(fd);
  }

  void cancel(fd_t fd) override { remove(fd); }

  std::vector<PollResult> wait(int timeout_ms) override {
    fd_set readset;
    fd_set writeset;

    FD_ZERO(&readset);
    FD_ZERO(&writeset);

    fd_t max_fd = 0;

    {
      std::lock_guard lock(mutex_);

      for (auto &[fd, ev] : fd_set_map_) {
        if ((ev & PollEvent::Read) == PollEvent::Read)
          FD_SET(fd, &readset);

        if ((ev & PollEvent::Write) == PollEvent::Write)
          FD_SET(fd, &writeset);

        if (fd > max_fd)
          max_fd = fd;
      }
    }

    timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    select(static_cast<int>(max_fd + 1), &readset, &writeset, nullptr, &tv);

    std::vector<PollResult> results;

    std::lock_guard lock(mutex_);

    for (auto &[fd, ev] : fd_set_map_) {
      PollEvent mask = PollEvent::None;

      if (FD_ISSET(fd, &readset))
        mask = mask | PollEvent::Read;

      if (FD_ISSET(fd, &writeset))
        mask = mask | PollEvent::Write;

      results.push_back({fd, mask});
    }

    return results;
  }

private:
  std::unordered_map<fd_t, PollEvent> fd_set_map_;
  std::mutex mutex_;
};

} // namespace net
