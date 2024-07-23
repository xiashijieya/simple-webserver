#include <string>

#include "http_request.h"
#include "http_response.h"
#include "mini_test.h"
#include "router.h"
#include "static_file_handler.h"
#include "test_config.h"

using namespace sws;

namespace {

HttpRequest request_with(const std::string& method, const std::string& path) {
    HttpRequest request;
    Buffer buf;
    std::string raw = method + " " + path + " HTTP/1.1\r\n\r\n";
    buf.append(raw);
    request.parse(&buf);
    return request;
}

}

TEST_CASE(router_matches_registered_route) {
    Router router;
    router.add_route("GET", "/hello", [](const HttpRequest&) {
        HttpResponse response(200);
        response.set_body("hi");
        return response;
    });

    HttpResponse response = router.route(request_with("GET", "/hello"));
    CHECK_EQ(response.status(), 200);
}

TEST_CASE(router_returns_404_for_unknown_path) {
    Router router;
    HttpResponse response = router.route(request_with("GET", "/nope"));
    CHECK_EQ(response.status(), 404);
}

TEST_CASE(router_returns_405_for_wrong_method) {
    Router router;
    router.add_route("GET", "/hello", [](const HttpRequest&) {
        return HttpResponse(200);
    });

    HttpResponse response = router.route(request_with("POST", "/hello"));
    CHECK_EQ(response.status(), 405);
}

TEST_CASE(response_serialize_has_status_and_length) {
    HttpResponse response(200);
    response.set_body("abcd");

    std::string text = response.serialize(true);
    CHECK(text.find("HTTP/1.1 200 OK\r\n") == 0);
    CHECK(text.find("Content-Length: 4\r\n") != std::string::npos);
    CHECK(text.find("Connection: keep-alive\r\n") != std::string::npos);
    CHECK(text.size() > 4);
}

TEST_CASE(static_file_serves_index_and_mime) {
    StaticFileHandler files(SWS_TEST_FIXTURES_DIR);
    HttpResponse response = files.serve(request_with("GET", "/index.html"));

    CHECK_EQ(response.status(), 200);
    CHECK(response.serialize(true).find("text/html") != std::string::npos);
}

TEST_CASE(static_file_missing_is_404) {
    StaticFileHandler files(SWS_TEST_FIXTURES_DIR);
    HttpResponse response = files.serve(request_with("GET", "/no-such.file"));
    CHECK_EQ(response.status(), 404);
}

TEST_CASE(static_file_blocks_path_traversal) {
    StaticFileHandler files(SWS_TEST_FIXTURES_DIR);
    HttpResponse response = files.serve(request_with("GET", "/../secret.txt"));
    CHECK(response.status() == 400 || response.status() == 404);
}

TEST_CASE(static_file_rejects_post) {
    StaticFileHandler files(SWS_TEST_FIXTURES_DIR);
    HttpResponse response = files.serve(request_with("POST", "/index.html"));
    CHECK_EQ(response.status(), 405);
}
