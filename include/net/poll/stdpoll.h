#include "net/detail/socket_handle.h"
#include "net/poll/ipoll.h"
#include <poll.h>
#include <vector>

namespace net {

class StdPoll : public detail::IPoll {
public:
  void add(detail::SocketDescriptorHandle::Handle fd,
           PollEvent events) override;
  void modify(detail::SocketDescriptorHandle::Handle fd,
              PollEvent events) override;
  void remove(detail::SocketDescriptorHandle::Handle fd) override;

  std::vector<PollResult> wait(int timeout_ms) override;

private:
  std::vector<pollfd> fds_;
};

} // namespace net
