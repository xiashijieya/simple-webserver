#ifndef SWS_HTTP_CONNECTION_H
#define SWS_HTTP_CONNECTION_H

#include <winsock2.h>

#include <functional>

#include "http_request.h"
#include "http_response.h"

namespace sws {

typedef std::function<HttpResponse(const HttpRequest&)> request_callback;

class HttpConnection {
public:
    HttpConnection(SOCKET fd, request_callback callback);

    void serve();

private:
    bool recv_request(HttpRequest* request);
    void send_response(const HttpResponse& response, bool keep_alive);

    SOCKET fd_;
    Buffer input_;
    request_callback callback_;
};

} // namespace sws

#endif
