#include "catch2/catch_test_macros.hpp"
#include "net/core/endpoint.h"
#include "net/detail/ip_address.h"

using namespace net;

TEST_CASE("Endpoint construction from string and IpAddress", "[endpoint]") {
  Endpoint ep1("127.0.0.1", 8080);
  REQUIRE(ep1.port() == 8080);
  REQUIRE(ep1.data() != nullptr);
  REQUIRE(ep1.size() == sizeof(sockaddr_in));

  detail::IpAddress ip("127.0.0.1");
  Endpoint ep2(ip, 12345);
  REQUIRE(ep2.port() == 12345);
  REQUIRE(ep2.data() != nullptr);
}

TEST_CASE("Endpoint size management", "[endpoint]") {
  Endpoint ep("127.0.0.1", 0);
  auto *size_ptr = ep.size_ptr();
  REQUIRE(size_ptr != nullptr);
  *size_ptr = 0;
  REQUIRE(ep.size() == 0);
  ep.set_size(sizeof(sockaddr_storage));
  REQUIRE(ep.size() == sizeof(sockaddr_storage));
}

TEST_CASE("Endpoint to_string", "[endpoint]") {
  Endpoint ep("127.0.0.1", 8080);
  std::string s = ep.to_string();
  REQUIRE(s.find("127.0.0.1") != std::string::npos);
  REQUIRE(s.find("8080") != std::string::npos);
}
