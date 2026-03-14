#include "net/protocol/tcp/tcp_acceptor.h"
#include "net/coroutine/async_operation.h"
#include "net/detail/socket_flags.h"
#include <memory>
#include <utility>

namespace net {
task<std::unique_ptr<IConnection>> TcpAcceptor::async_accept() {
  for (;;) {
    Endpoint peer;

    if (auto socket = socket_.accept(peer); socket) {
      co_return std::make_unique<TcpConnection>(std::move(*socket), peer,
                                                reactor_);
    }

    const auto err = detail::last_socket_error();

    if (detail::is_interrupted(err))
      continue;

    if (!detail::is_would_block(err))
      throw std::system_error(err, detail::socket_category(), "accept failed");

    // wait until socket readable
    co_await async_operation(reactor_, socket_.native_handle(),
                             PollEvent::Read);
  }
}
// task<std::unique_ptr<IConnection>> TcpAcceptor::async_accept() {
//   for (;;) {
//     Endpoint peer;
//
//     auto socket = socket_.accept(peer);
//
//     if (socket.has_value()) {
//
//       co_return std::make_unique<TcpConnection>(std::move(*socket), peer,
//                                                 reactor_);
//     }
//
//     const auto err = detail::last_socket_error();
//
//     if (detail::is_interrupted(err))
//       continue;
//
//     if (detail::is_would_block(err)) {
//       co_await async_operation(reactor_, socket_.native_handle(),
//                                PollEvent::Read);
//       continue;
//     }
//     throw std::system_error(err, detail::socket_category());
//   }
// }
//
} // namespace net
