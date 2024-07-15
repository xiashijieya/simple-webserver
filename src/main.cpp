#include <winsock2.h>
#include <windows.h>

#include <string>

#include "buffer.h"
#include "iterative_server.h"
#include "logger.h"
#include "socket_ops.h"

namespace {

BOOL WINAPI ctrl_handler(DWORD) {
    sws::request_stop();
    return TRUE;
}

void handle_connection(SOCKET fd) {
    sws::set_rcv_timeout(fd, 10000);

    sws::Buffer in;
    int n = in.read_fd(fd);
    if (n > 0) {
        std::string request = in.retrieve_all_as_string();
        LOG_INFO("recv %d bytes from %s: %.40s",
                 n, sws::peer_addr(fd).c_str(), request.c_str());

        std::string body = "hello from SimpleWebServer\n";
        std::string response =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: " + std::to_string(body.size()) + "\r\n"
            "Connection: close\r\n"
            "\r\n" + body;
        send(fd, response.data(), static_cast<int>(response.size()), 0);
    }

    sws::close_socket(fd);
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
