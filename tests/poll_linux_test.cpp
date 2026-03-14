#include "net/poll/poll_factory.h"
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <sys/socket.h>
#include <unistd.h>

using namespace net;

TEST_CASE("LinuxPollReactor detects read readiness", "[poll][linux]") {
  auto reactor = CreateDefaultReactor();

  int sv[2];
  REQUIRE(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);

  int sender = sv[0];
  int receiver = sv[1];

  // Register receiver socket for read event
  reactor->add(receiver, PollEvent::Read);

  // Write data to sender to trigger readiness
  std::string msg = "hello_poll";

  ssize_t n = ::write(sender, msg.data(), msg.size());
  REQUIRE(n == static_cast<ssize_t>(msg.size()));

  // Poll wait (timeout small but nonzero for determinism)
  auto results = reactor->wait(1000);

  bool found = false;

  for (auto &r : results) {
    if (r.fd == receiver && (r.events & PollEvent::Read) == PollEvent::Read) {
      found = true;
      break;
    }
  }

  REQUIRE(found);

  close(sender);
  close(receiver);
}
