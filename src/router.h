#ifndef SWS_ROUTER_H
#define SWS_ROUTER_H

#include <functional>
#include <string>
#include <vector>

#include "http_request.h"
#include "http_response.h"

namespace sws {

class Router {
public:
    typedef std::function<HttpResponse(const HttpRequest&)> route_handler;

    void add_route(const std::string& method,
                   const std::string& path,
                   route_handler handler);
    HttpResponse route(const HttpRequest& request) const;

private:
    struct Route {
        std::string method;
        std::string path;
        route_handler handler;
    };

    std::vector<Route> routes_;
};

} // namespace sws

#endif
