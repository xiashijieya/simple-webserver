#include "server.h"

#include <atomic>

#include "logger.h"
#include "socket_ops.h"

namespace sws {

namespace {
std::atomic<bool> g_stopping(false);
}

bool is_stopping() {
    return g_stopping.load();
}

void request_stop() {
    g_stopping.store(true);
}

Server::Server(uint16_t port, connection_handler handler)
    : port_(port), handler_(handler), listen_fd_(INVALID_SOCKET) {
}

Server::~Server() {
    close_socket(listen_fd_);
}

void Server::run() {
    listen_fd_ = create_listen_socket(port_, true);
    if (listen_fd_ == INVALID_SOCKET) return;

    LOG_INFO("%s model serving on port %u", model_name(), port_);

    while (!is_stopping()) {
        sockaddr_in client_addr;
        int addr_len = sizeof(client_addr);
        SOCKET fd = accept(listen_fd_,
                           reinterpret_cast<sockaddr*>(&client_addr), &addr_len);
        if (fd == INVALID_SOCKET) {
            int err = socket_error();
            if (error_would_block(err)) {
                Sleep(20);
                continue;
            }
            if (is_stopping()) break;
            LOG_WARN("accept failed %d", err);
            Sleep(20);
            continue;
        }
        on_accept(fd);
    }

    on_shutdown();
    close_socket(listen_fd_);
    listen_fd_ = INVALID_SOCKET;
    LOG_INFO("%s server stopped", model_name());
}

void Server::stop() {
    request_stop();
}

} // namespace sws
