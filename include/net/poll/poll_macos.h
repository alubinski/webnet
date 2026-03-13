#include "net/poll/ipoll.h"

#include <mutex>
#include <sys/event.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

namespace net {

class MacPoll : public detail::IPoll {
public:
  MacPoll() { kq_ = kqueue(); }

  ~MacPoll() override {
    if (kq_ >= 0)
      close(kq_);
  }

  void add(fd_t fd, PollEvent events) override {
    std::lock_guard lock(mutex_);

    struct kevent ev;

    if ((events & PollEvent::Read) == PollEvent::Read) {
      EV_SET(&ev, fd, EVFILT_READ, EV_ADD, 0, 0, nullptr);
      kevent(kq_, &ev, 1, nullptr, 0, nullptr);
    }

    if ((events & PollEvent::Write) == PollEvent::Write) {
      EV_SET(&ev, fd, EVFILT_WRITE, EV_ADD, 0, 0, nullptr);
      kevent(kq_, &ev, 1, nullptr, 0, nullptr);
    }

    registry_[fd] = events;
  }

  void modify(fd_t fd, PollEvent events) override { add(fd, events); }

  void remove(fd_t fd) override {
    std::lock_guard lock(mutex_);

    struct kevent ev;
    EV_SET(&ev, fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
    kevent(kq_, &ev, 1, nullptr, 0, nullptr);

    registry_.erase(fd);
  }

  void cancel(fd_t fd) override { remove(fd); }

  std::vector<PollResult> wait(int timeout_ms) override {
    std::vector<struct kevent> events(64);

    timespec ts;
    ts.tv_sec = timeout_ms / 1000;
    ts.tv_nsec = (timeout_ms % 1000) * 1000000;

    int n = kevent(kq_, nullptr, 0, events.data(), events.size(), &ts);

    std::vector<PollResult> results;

    if (n > 0) {
      for (int i = 0; i < n; i++) {
        PollEvent mask = PollEvent::None;

        if (events[i].filter == EVFILT_READ)
          mask = mask | PollEvent::Read;

        if (events[i].filter == EVFILT_WRITE)
          mask = mask | PollEvent::Write;

        results.push_back({(fd_t)events[i].ident, mask});
      }
    }

    return results;
  }

private:
  int kq_{-1};
  std::unordered_map<fd_t, PollEvent> registry_;
  std::mutex mutex_;
};

} // namespace net
