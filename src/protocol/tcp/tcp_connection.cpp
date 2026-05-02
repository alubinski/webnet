#include "net/protocol/tcp/tcp_connection.h"
#include "net/detail/platform_error.h"
#include "net/poll/ipoll.h"
#include <coroutine>
#include <cstddef>
#include <iostream>
#include <stdexcept>

namespace net {

void TcpConnection::close() {
  if (!closed_) {
    closed_ = true;
    socket_.close();
  }
}

// task<void> TcpConnection::async_connect(const Endpoint &ep) {
//   socket_.connect(ep);
//
//   for (;;) {
//     auto err = detail::last_socket_error();
//
//     if (!detail::is_in_progress(err))
//       break;
//
//     co_await async_operation(reactor_, socket_.native_handle(),
//                              PollEvent::Write);
//   }
//
//   co_return;
// }

} // namespace net
//
