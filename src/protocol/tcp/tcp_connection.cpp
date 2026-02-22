#include "net/protocol/tcp/tcp_connection.h"
#include "net/detail/platform_error.h"
#include <coroutine>
#include <cstddef>
#include <iostream>

namespace net {

task<std::size_t> TcpConnection::async_read(std::span<std::byte> buffer) {
  std::cout << "async_read loop enter\n";
  while (true) {

    auto received = socket_.receive(buffer);

    if (received > 0) {
      co_return static_cast<std::size_t>(received);
    }

    if (received == 0) {
      // peer performed orderly shutdown
      co_return 0;
    }

    auto err = detail::last_socket_error();

    if (detail::is_would_block(err)) {

      struct ReadAwaiter {
        TcpConnection &self;

        bool await_ready() const noexcept { return false; }

        void await_suspend(std::coroutine_handle<> h) {
          self.read_awaiting_ = h;
        }

        void await_resume() const noexcept {}
      };

      std::cout << "suspending coroutine\n";
      co_await ReadAwaiter{*this};
      continue;
    }

    throw std::runtime_error("recv failed");
  }
}

task<void> TcpConnection::async_write(std::span<const std::byte> buffer) {
  auto remaining = buffer;

  while (!remaining.empty()) {

    int sent = socket_.send(remaining);

    if (sent > 0) {
      remaining = remaining.subspan(sent);
      continue;
    }

    if (sent == 0) {
      throw std::runtime_error("connection closed");
    }

    auto err = detail::last_socket_error();

    if (detail::is_would_block(err)) {

      struct WriteAwaiter {
        TcpConnection &self;

        bool await_ready() const noexcept { return false; }

        void await_suspend(std::coroutine_handle<> h) {
          self.write_awaiting_ = h;
        }

        void await_resume() const noexcept {}
      };

      co_await WriteAwaiter{*this};
      continue;
    }

    throw std::runtime_error("send failed");
  }

  co_return;
}

void TcpConnection::notify_readable() {
  std::cout << "notify_readable called\n";
  if (read_awaiting_) {
    auto h = read_awaiting_;
    read_awaiting_ = {};
    h.resume();
  }
}

void TcpConnection::notify_writable() {
  if (write_buffer_.empty())
    return;

  try {
    auto n = socket_.send(write_buffer_);

    write_buffer_.erase(write_buffer_.begin(), write_buffer_.begin() + n);

    if (write_buffer_.empty() && write_awaiting_) {
      auto h = write_awaiting_;
      write_awaiting_ = {};
      h.resume();
    }

  } catch (...) {
    close();
  }
}

void TcpConnection::close() {
  if (!closed_) {
    closed_ = true;
    socket_.close();
  }
}

task<void> TcpConnection::async_connect(const Endpoint &ep) {
  socket_.connect(ep);

  while (true) {
    auto err = detail::last_socket_error();

    if (!detail::is_in_progress(err))
      break;

    struct WriteAwaiter {
      TcpConnection &self;

      bool await_ready() const noexcept { return false; }

      void await_suspend(std::coroutine_handle<> h) {
        self.write_awaiting_ = h;
      }

      void await_resume() const noexcept {}
    };

    co_await WriteAwaiter{*this};
  }

  co_return;
}

} // namespace net
//
