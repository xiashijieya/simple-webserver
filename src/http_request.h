#ifndef SWS_HTTP_REQUEST_H
#define SWS_HTTP_REQUEST_H

#include <cstddef>
#include <map>
#include <string>

#include "buffer.h"

namespace sws {

enum ParseState {
    parse_incomplete,
    parse_complete,
    parse_bad
};

class HttpRequest {
public:
    static const size_t max_body_size = 10 * 1024 * 1024;

    HttpRequest();

    ParseState parse(Buffer* buf);
    void reset();

    const std::string& method() const { return method_; }
    const std::string& path() const { return path_; }
    const std::string& version() const { return version_; }
    const std::string& body() const { return body_; }
    std::string get_header(const std::string& key) const;
    std::string get_query(const std::string& key) const;
    size_t content_length() const;
    bool keep_alive() const;

private:
    ParseState parse_body(Buffer* buf);
    bool parse_request_line(const std::string& line);
    bool parse_header_line(const std::string& line);

    std::string method_;
    std::string path_;
    std::string version_;
    std::map<std::string, std::string> headers_;
    std::map<std::string, std::string> query_;
    std::string body_;
    bool headers_done_;
};

} // namespace sws

#endif
