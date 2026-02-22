#include "net/protocol/tcp/tcp_acceptor.h"
#include "net/detail/socket_flags.h"
#include <memory>

namespace net {
task<std::unique_ptr<IConnection>> TcpAcceptor::async_accept() {
  struct Awaiter {
    TcpAcceptor &self;

    bool await_ready() noexcept { return false; }

    void await_suspend(std::coroutine_handle<> h) noexcept {
      self.accept_awaiting_ = h;
    }

    void await_resume() noexcept {}
  };

  while (true) {
    Endpoint peer;

    auto socket = socket_.accept(peer);

    if (socket.is_valid()) {
      socket.setBlocking(detail::SocketFlags::BlockingType::NonBlocking);

      co_return std::make_unique<TcpConnection>(std::move(socket), peer);
    }

    int err = detail::last_socket_error();

    if (detail::is_interrupted(err))
      continue;

    if (detail::is_would_block(err)) {
      co_await Awaiter{*this};
      continue;
    }

    throw std::system_error(err, detail::socket_category(),
                            "tcp accept failed");
  }
}

void TcpAcceptor::notify_readable() {
  if (accept_awaiting_ && !accept_awaiting_.done()) {
    auto h = accept_awaiting_;
    accept_awaiting_ = {};
    h.resume();
  }
}
} // namespace net
