#include "net/poll/epoll_context.h"
#include "net/server/http_server.h"
#include "net/server/http_worker.h"
#include <cstdlib>
#include <iostream>

int main(int argc, char *argv[]) {
  uint16_t port = 8080;
  if (argc > 1) {
    port = static_cast<uint16_t>(std::atoi(argv[1]));
  }

  net::http::HttpServer server{{"0.0.0.0", port}};
  server.serve();

  // net::http::HttpWorker worker;
  // worker.init({"0.0.0.0", port});

  // std::cin.get();
}
