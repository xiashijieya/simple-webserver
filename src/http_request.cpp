#include "http_request.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace sws {

namespace {

std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

std::string trim(const std::string& s) {
    size_t begin = s.find_first_not_of(" \t");
    if (begin == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t");
    return s.substr(begin, end - begin + 1);
}

} // namespace

HttpRequest::HttpRequest() : headers_done_(false) {
}

void HttpRequest::reset() {
    method_.clear();
    path_.clear();
    version_.clear();
    headers_.clear();
    body_.clear();
    headers_done_ = false;
}

ParseState HttpRequest::parse(Buffer* buf) {
    while (true) {
        const char* crlf = buf->find_crlf();
        if (crlf == NULL) {
            return headers_done_ ? parse_body(buf) : parse_incomplete;
        }

        size_t line_len = static_cast<size_t>(crlf - buf->peek());
        std::string line = buf->retrieve_as_string(line_len);
        buf->retrieve(2);

        if (method_.empty()) {
            if (!parse_request_line(line)) return parse_bad;
        } else if (line.empty()) {
            headers_done_ = true;
            return parse_body(buf);
        } else if (!parse_header_line(line)) {
            return parse_bad;
        }
    }
}

ParseState HttpRequest::parse_body(Buffer* buf) {
    size_t expected = content_length();
    if (expected > max_body_size) return parse_bad;
    if (buf->readable_bytes() < expected) return parse_incomplete;
    body_ = buf->retrieve_as_string(expected);
    return parse_complete;
}

bool HttpRequest::parse_request_line(const std::string& line) {
    std::istringstream iss(line);
    std::string method, path, version;
    if (!(iss >> method >> path >> version)) return false;
    if (version.compare(0, 5, "HTTP/") != 0) return false;
    if (path.empty() || path[0] != '/') return false;

    size_t query = path.find('?');
    if (query != std::string::npos) path = path.substr(0, query);

    method_ = method;
    path_ = path;
    version_ = version;
    return true;
}

bool HttpRequest::parse_header_line(const std::string& line) {
    size_t colon = line.find(':');
    if (colon == std::string::npos) return false;

    std::string key = trim(line.substr(0, colon));
    std::string value = trim(line.substr(colon + 1));
    if (key.empty()) return false;

    headers_[to_lower(key)] = value;
    return true;
}

std::string HttpRequest::get_header(const std::string& key) const {
    std::map<std::string, std::string>::const_iterator it =
        headers_.find(to_lower(key));
    return it == headers_.end() ? "" : it->second;
}

size_t HttpRequest::content_length() const {
    std::string value = get_header("Content-Length");
    if (value.empty()) return 0;

    size_t result = 0;
    for (size_t i = 0; i < value.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(value[i]))) return 0;
        result = result * 10 + static_cast<size_t>(value[i] - '0');
    }
    return result;
}

bool HttpRequest::keep_alive() const {
    std::string connection = to_lower(get_header("Connection"));
    if (version_ == "HTTP/1.1") return connection != "close";
    return connection == "keep-alive";
}

} // namespace sws
