#include <winsock2.h>
#include <windows.h>

#include <cstdint>
#include <string>

#include "http_connection.h"
#include "iterative_server.h"
#include "logger.h"
#include "router.h"
#include "socket_ops.h"

namespace {

sws::Router build_router() {
    sws::Router router;

    router.add_route("GET", "/hello",
        [](const sws::HttpRequest&) {
            sws::HttpResponse response(200);
            response.set_content_type("text/plain");
            response.set_body("hello from SimpleWebServer\n");
            return response;
        });

    router.add_route("POST", "/echo",
        [](const sws::HttpRequest& request) {
            sws::HttpResponse response(200);
            std::string content_type = request.get_header("Content-Type");
            if (content_type.empty()) content_type = "text/plain";
            response.set_content_type(content_type);
            response.set_body(request.body());
            return response;
        });

    return router;
}

void handle_connection(SOCKET fd, const sws::Router* router) {
    sws::HttpConnection connection(
        fd, [router](const sws::HttpRequest& request) {
            return router->route(request);
        });
    connection.serve();
}

BOOL WINAPI ctrl_handler(DWORD) {
    sws::request_stop();
    return TRUE;
}

} // namespace

int main() {
    sws::WinsockInit winsock;
    if (!winsock.ok()) return 1;

    SetConsoleCtrlHandler(ctrl_handler, TRUE);

    sws::Router router = build_router();

    uint16_t port = 8080;
    sws::IterativeServer server(
        port, [&router](SOCKET fd) { handle_connection(fd, &router); });
    LOG_INFO("press ctrl c to stop");
    server.run();
    return 0;
}
