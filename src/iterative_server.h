#ifndef SWS_ITERATIVE_SERVER_H
#define SWS_ITERATIVE_SERVER_H

#include "server.h"

namespace sws {

class IterativeServer : public Server {
public:
    IterativeServer(uint16_t port, connection_handler handler)
        : Server(port, handler) {}

protected:
    const char* model_name() const { return "iterative"; }

    void on_accept(SOCKET fd) {
        handler_(fd);
    }
};

} // namespace sws

#endif
