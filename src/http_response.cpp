#include "http_response.h"

#include <sstream>

namespace sws {

HttpResponse::HttpResponse(int status) : status_(status) {
}

void HttpResponse::set_status(int status) {
    status_ = status;
}

void HttpResponse::set_header(const std::string& key, const std::string& value) {
    headers_[key] = value;
}

void HttpResponse::set_content_type(const std::string& content_type) {
    set_header("Content-Type", content_type);
}

void HttpResponse::set_body(const std::string& body) {
    body_ = body;
}

const char* HttpResponse::status_text() const {
    switch (status_) {
        case 200: return "OK";
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        default:  return "Unknown";
    }
}

std::string HttpResponse::serialize(bool keep_alive) const {
    std::ostringstream out;
    out << "HTTP/1.1 " << status_ << " " << status_text() << "\r\n";
    out << "Server: SimpleWebServer\r\n";
    out << "Content-Length: " << body_.size() << "\r\n";
    out << "Connection: " << (keep_alive ? "keep-alive" : "close") << "\r\n";

    for (std::map<std::string, std::string>::const_iterator it = headers_.begin();
         it != headers_.end(); ++it) {
        out << it->first << ": " << it->second << "\r\n";
    }

    out << "\r\n";
    std::string head = out.str();
    return head + body_;
}

} // namespace sws
