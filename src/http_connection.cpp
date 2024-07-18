#include "http_connection.h"

#include "logger.h"
#include "server.h"
#include "socket_ops.h"

namespace sws {

namespace {
const int k_idle_timeout_ms = 10000;
const size_t k_max_requests_per_connection = 1000;
}

HttpConnection::HttpConnection(SOCKET fd, request_callback callback)
    : fd_(fd), callback_(callback) {
}

void HttpConnection::serve() {
    set_blocking(fd_);
    set_rcv_timeout(fd_, k_idle_timeout_ms);

    HttpRequest request;
    size_t request_count = 0;

    while (!is_stopping()) {
        if (!recv_request(&request)) {
            close_socket(fd_);
            return;
        }

        ++request_count;
        bool keep_alive = request.keep_alive()
                          && request_count < k_max_requests_per_connection;

        HttpResponse response = callback_(request);
        send_response(response, keep_alive);

        if (!keep_alive) {
            close_socket(fd_);
            return;
        }
        request.reset();
    }

    close_socket(fd_);
}

bool HttpConnection::recv_request(HttpRequest* request) {
    while (true) {
        ParseState state = request->parse(&input_);
        if (state == parse_complete) return true;
        if (state == parse_bad) {
            HttpResponse bad(400);
            bad.set_content_type("text/plain");
            bad.set_body("400 Bad Request\n");
            send_response(bad, false);
            return false;
        }

        int n = input_.read_fd(fd_);
        if (n == 0) {
            LOG_DEBUG("peer closed before request complete");
            return false;
        }
        if (n == SOCKET_ERROR) {
            int err = socket_error();
            if (error_timeout(err)) {
                LOG_DEBUG("idle connection timeout");
            } else {
                LOG_WARN("recv failed %d from %s", err, peer_addr(fd_).c_str());
            }
            return false;
        }
    }
}

void HttpConnection::send_response(const HttpResponse& response, bool keep_alive) {
    std::string data = response.serialize(keep_alive);

    size_t sent_total = 0;
    while (sent_total < data.size()) {
        int n = send(fd_, data.data() + sent_total,
                     static_cast<int>(data.size() - sent_total), 0);
        if (n == SOCKET_ERROR) {
            LOG_WARN("send failed %d to %s", socket_error(), peer_addr(fd_).c_str());
            break;
        }
        sent_total += static_cast<size_t>(n);
    }
}

} // namespace sws
