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

int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// url地址的解析规则
// https://www.bilibili.com/video/xxxx?trackid=web_pegasus_0.router-web-pegasus-2479516-gfll4.1790260401874.217#sectionxxxxxx
// scheme |      host      |   path   |                            query                                      |  section3   |
// query 里的+要转成空格, %开头的是两个16进制描述的一个字符
// URL 只支持 ASCII 码传输。
// URL encoding 编码字符到能传输的格式。
// URL encoding 使用 % 加上两个十六进制数编码不支持的字符。
// URL 不能含有空格，URL encoding 替换空格成 %20。
std::string url_decode(const std::string& s) {
    std::string result;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '+') {
            result += ' ';
        } else if (s[i] == '%' && i + 2 < s.size()) {
            int high = hex_value(s[i + 1]);
            int low = hex_value(s[i + 2]);
            if (high < 0 || low < 0) {
                result += s[i];
            } else {
                result += static_cast<char>(high * 16 + low);
                i += 2;
            }
        } else {
            result += s[i];
        }
    }
    return result;
}

} // namespace

HttpRequest::HttpRequest() : headers_done_(false) {
}

void HttpRequest::reset() {
    method_.clear();
    path_.clear();
    version_.clear();
    headers_.clear();
    query_.clear();
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

    std::string query_string;
    size_t query = path.find('?');
    if (query != std::string::npos) {
        query_string = path.substr(query + 1);
        path = path.substr(0, query);
    }

    method_ = method;
    path_ = path;
    version_ = version;

    size_t pair_begin = 0;
    while (pair_begin <= query_string.size()) {
        size_t pair_end = query_string.find('&', pair_begin);
        if (pair_end == std::string::npos) pair_end = query_string.size();

        std::string pair = query_string.substr(pair_begin, pair_end - pair_begin);
        size_t eq = pair.find('=');
        std::string key = eq == std::string::npos ? pair : pair.substr(0, eq);
        std::string value = eq == std::string::npos ? "" : pair.substr(eq + 1);
        if (!key.empty()) query_[url_decode(key)] = url_decode(value);

        if (pair_end == query_string.size()) break;
        pair_begin = pair_end + 1;
    }

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

std::string HttpRequest::get_query(const std::string& key) const {
    std::map<std::string, std::string>::const_iterator it = query_.find(key);
    return it == query_.end() ? "" : it->second;
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
