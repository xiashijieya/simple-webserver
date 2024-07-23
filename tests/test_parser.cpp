#include <string>

#include "buffer.h"
#include "http_request.h"
#include "mini_test.h"

using namespace sws;

namespace {

Buffer buffer_with(const std::string& text) {
    Buffer buf;
    buf.append(text);
    return buf;
}

}

TEST_CASE(parser_full_get_request) {
    Buffer buf = buffer_with(
        "GET /hello HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Connection: keep-alive\r\n"
        "\r\n");

    HttpRequest request;
    CHECK_EQ(request.parse(&buf), parse_complete);
    CHECK_EQ(request.method(), std::string("GET"));
    CHECK_EQ(request.path(), std::string("/hello"));
    CHECK_EQ(request.version(), std::string("HTTP/1.1"));
    CHECK_EQ(request.get_header("host"), std::string("localhost"));
    CHECK(request.keep_alive());
    CHECK_EQ(buf.readable_bytes(), static_cast<size_t>(0));
}

TEST_CASE(parser_waits_for_partial_data) {
    Buffer buf;
    HttpRequest request;

    buf.append(std::string("POST /echo HTTP/1.1\r\n"));
    CHECK_EQ(request.parse(&buf), parse_incomplete);

    buf.append(std::string("Content-Length: 4\r\n\r\nbody"));
    CHECK_EQ(request.parse(&buf), parse_complete);
    CHECK_EQ(request.method(), std::string("POST"));
    CHECK_EQ(request.body(), std::string("body"));
}

TEST_CASE(parser_headers_are_case_insensitive) {
    Buffer buf = buffer_with(
        "GET / HTTP/1.0\r\ncOnNeCtIoN: KeEp-AlIvE\r\n\r\n");

    HttpRequest request;
    CHECK_EQ(request.parse(&buf), parse_complete);
    CHECK_EQ(request.get_header("CONNECTION"), std::string("KeEp-AlIvE"));
    CHECK(request.keep_alive());
}

TEST_CASE(parser_http10_defaults_to_close) {
    Buffer buf = buffer_with("GET / HTTP/1.0\r\n\r\n");
    HttpRequest request;
    CHECK_EQ(request.parse(&buf), parse_complete);
    CHECK(!request.keep_alive());
}

TEST_CASE(parser_http11_explicit_close) {
    Buffer buf = buffer_with(
        "GET / HTTP/1.1\r\nConnection: close\r\n\r\n");
    HttpRequest request;
    CHECK_EQ(request.parse(&buf), parse_complete);
    CHECK(!request.keep_alive());
}

TEST_CASE(parser_rejects_bad_request_line) {
    Buffer buf = buffer_with("GARBAGE\r\n\r\n");
    HttpRequest request;
    CHECK_EQ(request.parse(&buf), parse_bad);
}

TEST_CASE(parser_strips_query_string) {
    Buffer buf = buffer_with("GET /search?q=cat HTTP/1.1\r\n\r\n");
    HttpRequest request;
    CHECK_EQ(request.parse(&buf), parse_complete);
    CHECK_EQ(request.path(), std::string("/search"));
}

TEST_CASE(parser_rejects_oversized_body) {
    Buffer buf = buffer_with(
        "POST / HTTP/1.1\r\nContent-Length: 99999999999\r\n\r\n");
    HttpRequest request;
    CHECK_EQ(request.parse(&buf), parse_bad);
}
