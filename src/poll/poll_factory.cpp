#include "net/poll/poll_factory.h"
#include "net/poll/ipoll.h"

#ifdef _WIN32
#include "net/poll/poll_windows.h"
#elif __APPLE__
#include "net/poll/poll_macos.h"
#else
#include "net/poll/poll_linux.h"
#endif

namespace net {

std::unique_ptr<detail::IPoll> CreateDefaultReactor() {
#ifdef _WIN32
  return std::make_unique<WindowsPoll>();
#elif __APPLE__
  return std::make_unique<MacPoll>();
#else
  return std::make_unique<LinuxPoll>();
#endif
}

} // namespace net
