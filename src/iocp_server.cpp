#include "iocp_server.h"

#include <winsock2.h>
#include <windows.h>
#include <mswsock.h>

#include <cstring>
#include <thread>
#include <vector>

#include "http_request.h"
#include "http_response.h"
#include "logger.h"
#include "socket_ops.h"

namespace sws {

namespace {

enum IoOp {
    op_accept,
    op_recv,
    op_send
};

const size_t k_max_requests_per_connection = 1000;

struct IoContext {
    OVERLAPPED ov;
    SOCKET fd;
    IoOp op;
    Buffer in;
    HttpRequest request;
    char accept_addr[2 * (sizeof(sockaddr_in) + 16)];
    WSABUF recv_buf;
    char scratch[16384];
    std::string send_data;
    WSABUF send_buf;
    bool keep_alive;
    size_t request_count;

    IoContext()
        : fd(INVALID_SOCKET), op(op_accept), keep_alive(false),
          request_count(0) {
        ZeroMemory(&ov, sizeof(ov));
        ZeroMemory(accept_addr, sizeof(accept_addr));
        recv_buf.len = sizeof(scratch);
        recv_buf.buf = scratch;
        send_buf.len = 0;
        send_buf.buf = NULL;
    }
};

} // namespace

struct IocpServer::Impl {
    Impl(uint16_t port, request_callback dispatch, size_t worker_count)
        : port(port), dispatch(dispatch), worker_count(worker_count),
          iocp(NULL), listen_fd(INVALID_SOCKET), accept_ex(NULL) {}

    uint16_t port;
    request_callback dispatch;
    size_t worker_count;
    HANDLE iocp;
    SOCKET listen_fd;
    LPFN_ACCEPTEX accept_ex;
    std::vector<std::thread> workers;

    bool init_accept_ex() {
        GUID guid = WSAID_ACCEPTEX;
        DWORD returned = 0;
        int rc = WSAIoctl(listen_fd, SIO_GET_EXTENSION_FUNCTION_POINTER,
                          &guid, sizeof(guid), &accept_ex, sizeof(accept_ex),
                          &returned, NULL, NULL);
        if (rc == SOCKET_ERROR) {
            LOG_ERROR("load AcceptEx failed %d", WSAGetLastError());
            return false;
        }
        return true;
    }

    void post_accept() {
        IoContext* ctx = new IoContext();
        ctx->fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (ctx->fd == INVALID_SOCKET) {
            delete ctx;
            return;
        }

        ZeroMemory(&ctx->ov, sizeof(ctx->ov));
        DWORD addr_len = sizeof(sockaddr_in) + 16;
        DWORD received = 0;
        BOOL ok = accept_ex(listen_fd, ctx->fd, ctx->accept_addr, 0,
                            addr_len, addr_len, &received, &ctx->ov);
        if (!ok && WSAGetLastError() != WSA_IO_PENDING) {
            if (!is_stopping()) {
                LOG_WARN("AcceptEx failed %d", WSAGetLastError());
            }
            close_socket(ctx->fd);
            delete ctx;
        }
    }

    void begin_recv(IoContext* ctx) {
        ctx->op = op_recv;
        ZeroMemory(&ctx->ov, sizeof(ctx->ov));
        ctx->recv_buf.len = sizeof(ctx->scratch);
        ctx->recv_buf.buf = ctx->scratch;

        DWORD flags = 0;
        DWORD bytes = 0;
        int rc = WSARecv(ctx->fd, &ctx->recv_buf, 1, &bytes, &flags,
                         &ctx->ov, NULL);
        if (rc == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
            destroy(ctx);
        }
    }

    void begin_send(IoContext* ctx) {
        if (ctx->send_data.empty()) {
            finish_send(ctx);
            return;
        }

        ctx->op = op_send;
        ZeroMemory(&ctx->ov, sizeof(ctx->ov));
        ctx->send_buf.len = static_cast<ULONG>(ctx->send_data.size());
        ctx->send_buf.buf = &ctx->send_data[0];

        DWORD bytes = 0;
        int rc = WSASend(ctx->fd, &ctx->send_buf, 1, &bytes, 0,
                         &ctx->ov, NULL);
        if (rc == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
            destroy(ctx);
        }
    }

    void process(IoContext* ctx) {
        ParseState state = ctx->request.parse(&ctx->in);
        if (state == parse_incomplete) {
            begin_recv(ctx);
            return;
        }

        if (state == parse_bad) {
            HttpResponse bad(400);
            bad.set_content_type("text/plain");
            bad.set_body("400 Bad Request\n");
            ctx->keep_alive = false;
            ctx->send_data = bad.serialize(false);
            begin_send(ctx);
            return;
        }

        ++ctx->request_count;
        ctx->keep_alive = ctx->request.keep_alive()
                          && ctx->request_count < k_max_requests_per_connection;

        HttpResponse response = dispatch(ctx->request);
        ctx->send_data = response.serialize(ctx->keep_alive);
        begin_send(ctx);
    }

    void finish_send(IoContext* ctx) {
        if (!ctx->keep_alive) {
            destroy(ctx);
            return;
        }

        ctx->request.reset();
        if (ctx->in.readable_bytes() > 0) {
            process(ctx);
        } else {
            begin_recv(ctx);
        }
    }

    void destroy(IoContext* ctx) {
        if (ctx->fd != INVALID_SOCKET) close_socket(ctx->fd);
        delete ctx;
    }

    void on_accept_done(IoContext* ctx) {
        setsockopt(ctx->fd, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
                   reinterpret_cast<const char*>(&listen_fd),
                   sizeof(listen_fd));
        CreateIoCompletionPort(reinterpret_cast<HANDLE>(ctx->fd),
                               iocp, 0, 0);

        if (!is_stopping()) post_accept();

        ctx->op = op_recv;
        begin_recv(ctx);
    }

    void on_failed(IoContext* ctx) {
        if (ctx->op == op_accept) {
            destroy(ctx);
            if (!is_stopping()) post_accept();
        } else {
            destroy(ctx);
        }
    }

    void worker_loop() {
        while (true) {
            DWORD bytes = 0;
            ULONG_PTR key = 0;
            LPOVERLAPPED pov = NULL;
            BOOL ok = GetQueuedCompletionStatus(
                iocp, &bytes, &key, &pov, INFINITE);

            if (pov == NULL) return;

            IoContext* ctx = CONTAINING_RECORD(pov, IoContext, ov);
            if (!ok) {
                on_failed(ctx);
                continue;
            }

            if (ctx->op == op_accept) {
                on_accept_done(ctx);
            } else if (ctx->op == op_recv) {
                if (bytes == 0) {
                    destroy(ctx);
                } else {
                    ctx->in.append(ctx->scratch, bytes);
                    process(ctx);
                }
            } else {
                finish_send(ctx);
            }
        }
    }
};

IocpServer::IocpServer(uint16_t port, request_callback dispatch,
                       size_t worker_count)
    : Server(port, connection_handler()),
      impl_(new Impl(port, dispatch, worker_count)) {
}

IocpServer::~IocpServer() {
    delete impl_;
}

void IocpServer::run() {
    impl_->listen_fd = create_listen_socket(impl_->port, true);
    if (impl_->listen_fd == INVALID_SOCKET) return;

    impl_->iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    if (impl_->iocp == NULL) {
        LOG_ERROR("create completion port failed %lu", GetLastError());
        close_socket(impl_->listen_fd);
        impl_->listen_fd = INVALID_SOCKET;
        return;
    }

    CreateIoCompletionPort(reinterpret_cast<HANDLE>(impl_->listen_fd),
                           impl_->iocp, 0, 0);

    if (!impl_->init_accept_ex()) {
        CloseHandle(impl_->iocp);
        close_socket(impl_->listen_fd);
        impl_->listen_fd = INVALID_SOCKET;
        return;
    }

    for (size_t i = 0; i < impl_->worker_count; ++i) {
        impl_->workers.push_back(
            std::thread(&Impl::worker_loop, impl_));
    }

    size_t accept_count = impl_->worker_count * 2;
    for (size_t i = 0; i < accept_count; ++i) impl_->post_accept();

    LOG_INFO("iocp model serving on port %u with %u workers",
             impl_->port, static_cast<unsigned>(impl_->worker_count));

    while (!is_stopping()) Sleep(100);

    close_socket(impl_->listen_fd);
    impl_->listen_fd = INVALID_SOCKET;

    for (size_t i = 0; i < impl_->worker_count; ++i) {
        PostQueuedCompletionStatus(impl_->iocp, 0, 0, NULL);
    }
    for (size_t i = 0; i < impl_->workers.size(); ++i) {
        impl_->workers[i].join();
    }
    impl_->workers.clear();

    CloseHandle(impl_->iocp);
    impl_->iocp = NULL;
    LOG_INFO("iocp server stopped");
}

} // namespace sws
