#include <winsock2.h>
#include <windows.h>

#include <cstdint>
#include <memory>
#include <string>

#include "http_connection.h"
#include "logger.h"
#include "router.h"
#include "socket_ops.h"
#include "static_file_handler.h"
#include "thread_server.h"

namespace {

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

BOOL WINAPI ctrl_handler(DWORD) {
    sws::request_stop();
    return TRUE;
}

} // namespace

int main() {
    sws::WinsockInit winsock;
    if (!winsock.ok()) return 1;

    SetConsoleCtrlHandler(ctrl_handler, TRUE);

    std::shared_ptr<sws::Router> router = build_router();
    std::shared_ptr<sws::StaticFileHandler> files(
        new sws::StaticFileHandler("wwwroot"));
    std::shared_ptr<sws::connection_handler> handler = build_handler(router, files);

    uint16_t port = 8080;
    sws::ThreadServer server(port, *handler);
    LOG_INFO("press ctrl c to stop");
    server.run();
    return 0;
}
