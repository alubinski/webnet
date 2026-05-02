#pragma once
#include <expected>
#include <format>
#include <string_view>
#include <vector>

enum class ParseError { InvalidRequestLine, InvalidHeader, Incomplite };

struct HttpHeader {
  std::string_view name;
  std::string_view value;
};

struct HttpRequest {
  std::string_view method;
  std::string_view uri;
  std::string_view version;
  std::vector<HttpHeader> headers;
};

template <> struct std::formatter<HttpHeader> {
  constexpr auto parse(format_parse_context &ctx) { return ctx.begin(); }

  auto format(const HttpHeader &h, format_context &ctx) const {
    return std::format_to(ctx.out(), "{}: {}", h.name, h.value);
  }
};

template <> struct std::formatter<HttpRequest> {
  constexpr auto parse(format_parse_context &ctx) { return ctx.begin(); }

  auto format(const HttpRequest &req, format_context &ctx) const {
    // Format the Request Line: GET /index.html HTTP/1.1
    auto out = std::format_to(ctx.out(), "{} {} {}\n", req.method, req.uri,
                              req.version);

    // Format all headers using the Header formatter we defined above
    for (const auto &h : req.headers) {
      out = std::format_to(out, "{}\n", h);
    }

    return out;
  }
};

// This populates the struct that your formatter knows how to print
inline auto parse_http(std::string_view raw_request)
    -> std::expected<HttpRequest, ParseError> {
  HttpRequest req;

  // 1. Find the end of the Request Line
  auto line_end = raw_request.find("\r\n");
  if (line_end == std::string_view::npos)
    return std::unexpected(ParseError::Incomplite);

  std::string_view request_line = raw_request.substr(0, line_end);

  // 2. Simple split of Method, URI, Version
  auto first_space = request_line.find(' ');
  auto second_space = request_line.find(' ', first_space + 1);

  if (first_space == std::string_view::npos ||
      second_space == std::string_view::npos)
    return std::unexpected(ParseError::InvalidRequestLine);

  req.method = request_line.substr(0, first_space);
  req.uri =
      request_line.substr(first_space + 1, second_space - first_space - 1);
  req.version = request_line.substr(second_space + 1);

  // 3. Parse Headers
  std::string_view header_section = raw_request.substr(line_end + 2);
  size_t pos = 0;
  while ((pos = header_section.find("\r\n")) != std::string_view::npos) {
    if (pos == 0)
      break; // Empty line means end of headers

    std::string_view line = header_section.substr(0, pos);
    auto colon = line.find(':');
    if (colon != std::string_view::npos) {
      req.headers.push_back({
          .name = line.substr(0, colon),
          .value = line.substr(colon + 2) // skip the ": "
      });
    }
    header_section = header_section.substr(pos + 2);
  }

  return req;
}
