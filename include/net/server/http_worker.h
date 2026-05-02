#pragma once

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "net/connection/iconnection.h"
#include "net/core/endpoint.h"
#include "net/coroutine/spawn.h"
#include "net/coroutine/task.h"
#include "net/poll/epoll_context.h"
#include "net/protocol/tcp/tcp_acceptor.h"
#include "net/protocol/tcp/tcp_socket.h"

namespace net::http {

class HttpWorker {
public:
  HttpWorker() : acceptor_socket() {}

  void init(Endpoint ep) {
    // std::cout << "[HttpWorker] init(): starting with endpoint\n";
    // queue init
    // epoll_context::get_instance().watch(fd_t fd, uint32_t events,
    //                                     std::coroutine_handle<> h);

    acceptor_socket.bind(ep);

    // std::cout << "[HttpWorker] bind(): success\n";
    acceptor_socket.listen();

    // std::cout << "[HttpWorker] listen(): socket is listening\n";
    //
    // std::cout << "[HttpWorker] starting accept loop\n";
    spawn(accept_client());
  };

  [[nodiscard]] auto accept_client() noexcept -> task<> {
    while (true) {
      std::cout << "Working\n";
      spawn(handle_client((co_await acceptor_socket.async_accept())));
    }
  }

  task<void> handle_client(std::shared_ptr<IConnection> conn);

  Endpoint local_endpoint() { return acceptor_socket.local_endpoint(); }

private:
  TcpAcceptor acceptor_socket;
};

} // namespace net::http
