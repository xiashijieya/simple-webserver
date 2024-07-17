#include <winsock2.h>
#include <windows.h>

#include <cstdint>

#include "http_connection.h"
#include "iterative_server.h"
#include "logger.h"
#include "socket_ops.h"

namespace {

sws::HttpResponse dispatch(const sws::HttpRequest& request) {
    LOG_INFO("%s %s", request.method().c_str(), request.path().c_str());

    sws::HttpResponse response(200);
    response.set_content_type("text/plain");
    response.set_body("hello from SimpleWebServer\n");
    return response;
}

void handle_connection(SOCKET fd) {
    sws::HttpConnection connection(fd, dispatch);
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

    uint16_t port = 8080;
    sws::IterativeServer server(port, handle_connection);
    LOG_INFO("press ctrl c to stop");
    server.run();
    return 0;
}
