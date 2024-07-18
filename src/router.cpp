#include "router.h"

namespace sws {

void Router::add_route(const std::string& method,
                       const std::string& path,
                       route_handler handler) {
    Route route;
    route.method = method;
    route.path = path;
    route.handler = handler;
    routes_.push_back(route);
}

HttpResponse Router::route(const HttpRequest& request) const {
    bool path_matched = false;

    for (size_t i = 0; i < routes_.size(); ++i) {
        const Route& route = routes_[i];
        if (route.path != request.path()) continue;

        path_matched = true;
        if (route.method == request.method()) {
            return route.handler(request);
        }
    }

    HttpResponse response(path_matched ? 405 : 404);
    response.set_content_type("text/plain");
    response.set_body(path_matched ? "405 Method Not Allowed\n"
                                   : "404 Not Found\n");
    return response;
}

} // namespace sws
