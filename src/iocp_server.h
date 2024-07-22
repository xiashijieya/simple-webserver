#ifndef SWS_IOCP_SERVER_H
#define SWS_IOCP_SERVER_H

#include <cstddef>

#include "http_connection.h"
#include "server.h"

namespace sws {

class IocpServer : public Server {
public:
    IocpServer(uint16_t port, request_callback dispatch, size_t worker_count);
    ~IocpServer();

    void run();

protected:
    const char* model_name() const { return "iocp"; }
    void on_accept(SOCKET) {}

private:
    struct Impl;
    Impl* impl_;
};

} // namespace sws

#endif
