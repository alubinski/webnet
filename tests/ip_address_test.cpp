#include "catch2/catch_test_macros.hpp"
#include "net/detail/ip_address.h"

#include <string>

using namespace net;

TEST_CASE("IpAddress IPv4 parsing", "[ip_address]") {
  using namespace net::detail;

  IpAddress ip("127.0.0.1");
  REQUIRE(ip.type() == IpAddress::Type::IPv4);
  REQUIRE(ip.to_string() == "127.0.0.1");
  REQUIRE(ip.family() == AF_INET);
  REQUIRE(ip.data() != nullptr);

  // Construct from raw data
  struct in_addr addr{};
  addr.s_addr = htonl(0x7f000001); // 127.0.0.1
  IpAddress ip2(&addr, IpAddress::Type::IPv4);
  REQUIRE(ip2.to_string() == "127.0.0.1");
}

TEST_CASE("IpAddress IPv6 parsing", "[ip_address]") {
  using namespace net::detail;

  IpAddress ip("::1");
  REQUIRE(ip.type() == IpAddress::Type::IPv6);
  REQUIRE(ip.to_string() == "::1");
  REQUIRE(ip.family() == AF_INET6);
  REQUIRE(ip.data() != nullptr);

  // Construct from raw data
  struct in6_addr addr6{};
  addr6.s6_addr[15] = 1; // ::1
  IpAddress ip2(&addr6, IpAddress::Type::IPv6);
  REQUIRE(ip2.to_string() == "::1");
}
