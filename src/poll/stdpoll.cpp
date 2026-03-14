#include "net/poll/stdpoll.h"
#include "net/detail/socket_handle.h"
#include <algorithm>

namespace net {

static short to_native(PollEvent e) {
  short result = 0;

  if ((static_cast<uint8_t>(e) & static_cast<uint8_t>(PollEvent::Read)))
    result |= POLLIN;

  if ((static_cast<uint8_t>(e) & static_cast<uint8_t>(PollEvent::Write)))
    result |= POLLOUT;

  return result;
}

void StdPoll::add(detail::SocketDescriptorHandle::Handle fd, PollEvent events) {
  pollfd p{};
  p.fd = fd;
  p.events = to_native(events);
  p.revents = 0;
  fds_.push_back(p);
}

void StdPoll::modify(detail::SocketDescriptorHandle::Handle fd,
                     PollEvent events) {
  for (auto &p : fds_) {
    if (p.fd == fd) {
      p.events = to_native(events);
      return;
    }
  }
}

void StdPoll::remove(detail::SocketDescriptorHandle::Handle fd) {
  fds_.erase(std::remove_if(fds_.begin(), fds_.end(),
                            [fd](const pollfd &p) { return p.fd == fd; }),
             fds_.end());
}

std::vector<PollResult> StdPoll::wait(int timeout_ms) {
  std::vector<PollResult> results;

  int ret = ::poll(fds_.data(), fds_.size(), timeout_ms);
  if (ret <= 0)
    return results;

  for (auto &p : fds_) {
    if (p.revents == 0)
      continue;

    PollEvent ev = PollEvent::None;

    if (p.revents & POLLIN)
      ev = ev | PollEvent::Read;

    if (p.revents & POLLOUT)
      ev = ev | PollEvent::Write;

    if (p.revents & POLLERR)
      ev = ev | PollEvent::Error;

    results.push_back({p.fd, ev});
    p.revents = 0;
  }

  return results;
}

} // namespace net
