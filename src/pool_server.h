#ifndef SWS_POOL_SERVER_H
#define SWS_POOL_SERVER_H

#include "server.h"
#include "thread_pool.h"

namespace sws {

class PoolServer : public Server {
public:
    PoolServer(uint16_t port, connection_handler handler, size_t thread_count)
        : Server(port, handler), pool_(thread_count) {}

protected:
    const char* model_name() const { return "pool"; }

    void on_accept(SOCKET fd) {
        pool_.submit([this, fd]() { handler_(fd); });
    }

    void on_shutdown() {
        pool_.shutdown();
    }

private:
    ThreadPool pool_;
};

} // namespace sws

#endif
