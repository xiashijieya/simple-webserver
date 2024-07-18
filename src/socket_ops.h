#ifndef SWS_SOCKET_OPS_H
#define SWS_SOCKET_OPS_H

#include <winsock2.h>
#include <ws2tcpip.h>

#include <cstdint>
#include <string>

namespace sws {

class WinsockInit {
public:
    WinsockInit();
    ~WinsockInit();
    bool ok() const { return ok_; }

private:
    bool ok_;
};

SOCKET create_listen_socket(uint16_t port, bool non_blocking, int backlog = 128);
void set_non_blocking(SOCKET fd);
void set_blocking(SOCKET fd);
void set_reuse_addr(SOCKET fd);
void set_rcv_timeout(SOCKET fd, int timeout_ms);
void close_socket(SOCKET fd);
std::string peer_addr(SOCKET fd);
int socket_error();
bool error_would_block(int error);
bool error_timeout(int error);

} // namespace sws

#endif
