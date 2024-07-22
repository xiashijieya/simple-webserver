#ifndef SWS_SERVER_H
#define SWS_SERVER_H

#include <winsock2.h>

#include <cstdint>
#include <functional>

namespace sws {

typedef std::function<void(SOCKET)> connection_handler;

class Server {
public:
    Server(uint16_t port, connection_handler handler);
    virtual ~Server();

    virtual void run();
    void stop();

protected:
    virtual const char* model_name() const = 0;
    virtual void on_accept(SOCKET fd) = 0;
    virtual void on_shutdown() {}

    uint16_t port_;
    connection_handler handler_;
    SOCKET listen_fd_;
};

bool is_stopping();
void request_stop();

} // namespace sws

#endif
