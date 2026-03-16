#pragma once
#include "net/server/http_parser.h"
#include <expected>
#include <format>
#include <string_view>
#include <vector>

enum class HttpStatus {
  OK = 200,
  BadRequest = 400,
  NotFound = 404,
  InternalError = 500
};

struct HttpResponse {
  HttpStatus status;
  std::string_view version = "HTTP/1.1";
  std::vector<HttpHeader> headers;
  std::string body;
};

template <> struct std::formatter<HttpStatus> {
  constexpr auto parse(format_parse_context &ctx) { return ctx.begin(); }

  auto format(HttpStatus s, format_context &ctx) const {
    std::string_view reason = "Unknown";
    switch (s) {
    case HttpStatus::OK:
      reason = "OK";
      break;
    case HttpStatus::BadRequest:
      reason = "Bad Request";
      break;
    case HttpStatus::NotFound:
      reason = "Not Found";
      break;
    case HttpStatus::InternalError:
      reason = "Internal Server Error";
      break;
    }
    return std::format_to(ctx.out(), "{} {}", static_cast<int>(s), reason);
  }
};

template <> struct std::formatter<HttpResponse> {
  // 1. The parse method must be constexpr and return the iterator
  constexpr auto parse(format_parse_context &ctx) { return ctx.begin(); }

  // 2. The format method must be const
  auto format(const HttpResponse &res, format_context &ctx) const {
    auto out = std::format_to(ctx.out(), "{} {}\r\n", res.version, res.status);
    for (const auto &h : res.headers) {
      out = std::format_to(out, "{}\r\n", h);
    }
    return std::format_to(out, "\r\n{}", res.body);
  }
};

enum class GenError { BufferTooSmall };

class HttpGenerator {
public:
  // Generates the raw bytes into 'out_buffer'
  static std::expected<std::string_view, GenError>
  serialize(const HttpResponse &res, std::span<char> out_buffer) {
    auto it = out_buffer.begin();
    auto end = out_buffer.end();

    try {
      // 1. Status Line: HTTP/1.1 200 OK\r\n
      auto res_line = std::format("{} {}\r\n", res.version, res.status);
      if (std::distance(it, end) < res_line.size())
        return std::unexpected(GenError::BufferTooSmall);
      it = std::copy(res_line.begin(), res_line.end(), it);

      // 2. Headers: Content-Length is usually mandatory
      for (const auto &h : res.headers) {
        auto h_line =
            std::format("{}\r\n", h); // Uses your existing Header formatter!
        if (std::distance(it, end) < h_line.size())
          return std::unexpected(GenError::BufferTooSmall);
        it = std::copy(h_line.begin(), h_line.end(), it);
      }

      // 3. Header/Body Separator
      if (std::distance(it, end) < 2)
        return std::unexpected(GenError::BufferTooSmall);
      *it++ = '\r';
      *it++ = '\n';

      // 4. Body
      if (std::distance(it, end) < res.body.size())
        return std::unexpected(GenError::BufferTooSmall);
      it = std::copy(res.body.begin(), res.body.end(), it);

      return std::string_view(out_buffer.data(),
                              std::distance(out_buffer.begin(), it));
    } catch (...) {
      return std::unexpected(GenError::BufferTooSmall);
    }
  }
};
