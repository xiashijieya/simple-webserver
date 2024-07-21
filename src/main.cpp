#include <winsock2.h>
#include <windows.h>

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "http_connection.h"
#include "iterative_server.h"
#include "logger.h"
#include "pool_server.h"
#include "router.h"
#include "socket_ops.h"
#include "static_file_handler.h"
#include "thread_server.h"

namespace {

void print_usage() {
    std::cerr << "usage: webserver [port] [model] [thread_count]\n"
              << "  model: iterative | thread | pool (default pool)\n";
}

std::shared_ptr<sws::Router> build_router() {
    std::shared_ptr<sws::Router> router(new sws::Router());

    router->add_route("GET", "/hello",
        [](const sws::HttpRequest&) {
            sws::HttpResponse response(200);
            response.set_content_type("text/plain");
            response.set_body("hello from SimpleWebServer\n");
            return response;
        });

    router->add_route("POST", "/echo",
        [](const sws::HttpRequest& request) {
            sws::HttpResponse response(200);
            std::string content_type = request.get_header("Content-Type");
            if (content_type.empty()) content_type = "text/plain";
            response.set_content_type(content_type);
            response.set_body(request.body());
            return response;
        });

    return router;
}

std::shared_ptr<sws::connection_handler>
build_handler(const std::shared_ptr<sws::Router>& router,
              const std::shared_ptr<sws::StaticFileHandler>& files) {
    std::shared_ptr<sws::connection_handler> handler(
        new sws::connection_handler(
            [router, files](SOCKET fd) {
                sws::HttpConnection connection(
                    fd, [router, files](const sws::HttpRequest& request) {
                        sws::HttpResponse response = router->route(request);
                        if (response.status() == 404) response = files->serve(request);
                        return response;
                    });
                connection.serve();
            }));
    return handler;
}

std::unique_ptr<sws::Server>
create_server(const std::string& model, uint16_t port, size_t thread_count,
              sws::connection_handler* handler) {
    if (model == "iterative") {
        return std::unique_ptr<sws::Server>(
            new sws::IterativeServer(port, *handler));
    }
    if (model == "thread") {
        return std::unique_ptr<sws::Server>(
            new sws::ThreadServer(port, *handler));
    }
    if (model == "pool") {
        return std::unique_ptr<sws::Server>(
            new sws::PoolServer(port, *handler, thread_count));
    }
    return std::unique_ptr<sws::Server>();
}

BOOL WINAPI ctrl_handler(DWORD) {
    sws::request_stop();
    return TRUE;
}

} // namespace

int main(int argc, char** argv) {
    uint16_t port = 8080;
    std::string model = "pool";
    size_t thread_count = std::thread::hardware_concurrency();
    if (thread_count == 0) thread_count = 4;

    if (argc >= 2) {
        long value = std::strtol(argv[1], NULL, 10);
        if (value <= 0 || value > 65535) {
            print_usage();
            return 1;
        }
        port = static_cast<uint16_t>(value);
    }
    if (argc >= 3) model = argv[2];
    if (argc >= 4) {
        long value = std::strtol(argv[3], NULL, 10);
        if (value <= 0) {
            print_usage();
            return 1;
        }
        thread_count = static_cast<size_t>(value);
    }

    sws::WinsockInit winsock;
    if (!winsock.ok()) return 1;

    SetConsoleCtrlHandler(ctrl_handler, TRUE);

    std::shared_ptr<sws::Router> router = build_router();
    std::shared_ptr<sws::StaticFileHandler> files(
        new sws::StaticFileHandler("wwwroot"));
    std::shared_ptr<sws::connection_handler> handler = build_handler(router, files);

    std::unique_ptr<sws::Server> server =
        create_server(model, port, thread_count, handler.get());
    if (!server) {
        print_usage();
        return 1;
    }

    LOG_INFO("model %s  port %u  threads %u",
             model.c_str(), port, static_cast<unsigned>(thread_count));
    LOG_INFO("press ctrl c to stop");
    server->run();
    return 0;
}
