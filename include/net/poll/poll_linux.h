#include "net/detail/socket_handle.h"
#include "net/poll/ipoll.h"
#include <mutex>
#include <sys/poll.h>
#include <unordered_map>

namespace net {

class LinuxPoll : public detail::IPoll {
public:
  LinuxPoll() = default;

  ~LinuxPoll() override = default;

  void add(fd_t fd, PollEvent events) override {
    std::lock_guard<std::mutex> lock{mutex_};

    poll_map_[fd] = pollfd{.fd = fd, .events = to_native(events), .revents = 0};
  }

  void modify(fd_t fd, PollEvent events) override {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = poll_map_.find(fd);
    if (it != poll_map_.end()) {
      it->second.events = to_native(events);
    }
  }

  void remove(fd_t fd) override {
    std::lock_guard<std::mutex> lock(mutex_);
    poll_map_.erase(fd);
  }

  void cancel(fd_t fd) override { remove(fd); }

  std::vector<PollResult> wait(int timeout_ms) override {
    std::vector<pollfd> fds;

    {
      std::lock_guard<std::mutex> lock(mutex_);

      fds.reserve(poll_map_.size());

      for (auto &[fd, pfd] : poll_map_) {
        fds.push_back(pfd);
      }
    }

    std::vector<PollResult> results;

    if (fds.empty())
      return results;

    int ret = ::poll(fds.data(), static_cast<nfds_t>(fds.size()), timeout_ms);

    if (ret <= 0)
      return results;

    for (auto &pfd : fds) {
      if (pfd.revents != 0) {
        results.push_back({.fd = pfd.fd, .events = from_native(pfd.revents)});
      }
    }

    return results;
  }

private:
  std::unordered_map<fd_t, pollfd> poll_map_;
  std::mutex mutex_;

  static short to_native(PollEvent e) {
    short mask = 0;

    if ((e & PollEvent::Read) == PollEvent::Read) {
      mask |= POLLIN;
    }

    if ((e & PollEvent::Write) == PollEvent::Write)
      mask |= POLLOUT;

    if ((e & PollEvent::Error) == PollEvent::Error)
      mask |= POLLERR;

    return mask;
  }

  static PollEvent from_native(short events) {
    PollEvent mask = PollEvent::None;

    if (events & POLLIN)
      mask = mask | PollEvent::Read;

    if (events & POLLOUT)
      mask = mask | PollEvent::Write;

    if (events & POLLERR)
      mask = mask | PollEvent::Error;

    return mask;
  }
};
} // namespace net
