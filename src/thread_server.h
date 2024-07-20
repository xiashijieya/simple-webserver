#ifndef SWS_THREAD_SERVER_H
#define SWS_THREAD_SERVER_H

#include <thread>

#include "server.h"

namespace sws {

class ThreadServer : public Server {
public:
    ThreadServer(uint16_t port, connection_handler handler)
        : Server(port, handler) {}

protected:
    const char* model_name() const { return "thread"; }

    void on_accept(SOCKET fd) {
        std::thread worker(&ThreadServer::serve_connection, this, fd);
        worker.detach();
    }

private:
    static void serve_connection(ThreadServer* self, SOCKET fd) {
        self->handler_(fd);
    }
};

} // namespace sws

#endif
