#include "socket_ops.h"

#include "logger.h"

namespace sws {

WinsockInit::WinsockInit() : ok_(false) {
    WSADATA data;
    int rc = WSAStartup(MAKEWORD(2, 2), &data);
    if (rc != 0) {
        LOG_ERROR("WSAStartup failed %d", rc);
        return;
    }
    ok_ = true;
}

WinsockInit::~WinsockInit() {
    if (ok_) WSACleanup();
}

void set_non_blocking(SOCKET fd) {
    unsigned long mode = 1;
    ioctlsocket(fd, FIONBIO, &mode);
}

void set_reuse_addr(SOCKET fd) {
    int on = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&on), sizeof(on));
}

void set_rcv_timeout(SOCKET fd, int timeout_ms) {
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO,
               reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms));
}

void close_socket(SOCKET fd) {
    if (fd != INVALID_SOCKET) closesocket(fd);
}

SOCKET create_listen_socket(uint16_t port, bool non_blocking, int backlog) {
    SOCKET fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd == INVALID_SOCKET) {
        LOG_ERROR("socket create failed %d", WSAGetLastError());
        return INVALID_SOCKET;
    }

    set_reuse_addr(fd);

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        LOG_ERROR("bind port %u failed %d", port, WSAGetLastError());
        close_socket(fd);
        return INVALID_SOCKET;
    }

    if (listen(fd, backlog) == SOCKET_ERROR) {
        LOG_ERROR("listen failed %d", WSAGetLastError());
        close_socket(fd);
        return INVALID_SOCKET;
    }

    if (non_blocking) set_non_blocking(fd);
    LOG_INFO("listen socket ready on port %u", port);
    return fd;
}

std::string peer_addr(SOCKET fd) {
    sockaddr_in addr;
    int len = sizeof(addr);
    if (getpeername(fd, reinterpret_cast<sockaddr*>(&addr), &len) == SOCKET_ERROR) {
        return "unknown";
    }
    char ip[INET_ADDRSTRLEN] = {0};
    inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));
    return std::string(ip) + ":" + std::to_string(ntohs(addr.sin_port));
}

int socket_error() {
    return WSAGetLastError();
}

bool error_would_block(int error) {
    return error == WSAEWOULDBLOCK;
}

bool error_timeout(int error) {
    return error == WSAETIMEDOUT;
}

} // namespace sws
