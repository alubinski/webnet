#include "net/poll/stdpoll.h"
#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <unistd.h> // pipe, write, close

using namespace net;

TEST_CASE("StdPoll detects readable event", "[poll]") {
  int fds[2];
  REQUIRE(pipe(fds) == 0);

  int read_fd = fds[0];
  int write_fd = fds[1];

  StdPoll poll;
  poll.add(read_fd, PollEvent::Read);

  // Write data to trigger readable event
  const char *msg = "hello";
  REQUIRE(write(write_fd, msg, std::strlen(msg)) > 0);

  auto events = poll.wait(1000);

  REQUIRE(events.size() == 1);
  REQUIRE(events[0].fd == read_fd);
  REQUIRE((events[0].events & PollEvent::Read) == PollEvent::Read);

  close(read_fd);
  close(write_fd);
}

TEST_CASE("StdPoll modify updates interest", "[poll]") {
  int fds[2];
  REQUIRE(pipe(fds) == 0);

  int read_fd = fds[0];
  int write_fd = fds[1];

  StdPoll poll;
  poll.add(read_fd, PollEvent::Read);

  // Change interest to Write (should not trigger read event)
  poll.modify(read_fd, PollEvent::Write);

  const char *msg = "data";
  REQUIRE(write(write_fd, msg, 4) > 0);

  auto events = poll.wait(200);

  REQUIRE(events.empty()); // no read event expected

  close(read_fd);
  close(write_fd);
}

TEST_CASE("StdPoll remove stops tracking fd", "[poll]") {
  int fds[2];
  REQUIRE(pipe(fds) == 0);

  int read_fd = fds[0];
  int write_fd = fds[1];

  StdPoll poll;
  poll.add(read_fd, PollEvent::Read);
  poll.remove(read_fd);

  const char *msg = "x";
  REQUIRE(write(write_fd, msg, 1) > 0);

  auto events = poll.wait(200);

  REQUIRE(events.empty());

  close(read_fd);
  close(write_fd);
}

TEST_CASE("StdPoll wait times out when no events", "[poll]") {
  int fds[2];
  REQUIRE(pipe(fds) == 0);

  int read_fd = fds[0];
  int write_fd = fds[1];

  StdPoll poll;
  poll.add(read_fd, PollEvent::Read);

  auto events = poll.wait(100);

  REQUIRE(events.empty());

  close(read_fd);
  close(write_fd);
}
