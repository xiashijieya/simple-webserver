#ifndef SWS_STATIC_FILE_HANDLER_H
#define SWS_STATIC_FILE_HANDLER_H

#include <string>

#include "http_request.h"
#include "http_response.h"

namespace sws {

class StaticFileHandler {
public:
    explicit StaticFileHandler(const std::string& root);

    HttpResponse serve(const HttpRequest& request) const;
    const std::string& root() const { return root_; }

private:
    std::string mime_type(const std::string& path) const;
    bool is_unsafe_path(const std::string& path) const;

    std::string root_;
};

} // namespace sws

#endif
