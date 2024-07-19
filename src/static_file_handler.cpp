#include "static_file_handler.h"

#include <fstream>
#include <sstream>

#include <windows.h>

namespace sws {

StaticFileHandler::StaticFileHandler(const std::string& root) : root_(root) {
    while (!root_.empty() && (root_[root_.size() - 1] == '/'
                              || root_[root_.size() - 1] == '\\')) {
        root_.erase(root_.size() - 1);
    }
}

HttpResponse StaticFileHandler::serve(const HttpRequest& request) const {
    if (request.method() != "GET" && request.method() != "HEAD") {
        HttpResponse response(405);
        response.set_header("Allow", "GET, HEAD");
        response.set_content_type("text/plain");
        response.set_body("405 Method Not Allowed\n");
        return response;
    }

    std::string url_path = request.path();
    if (is_unsafe_path(url_path)) {
        HttpResponse response(400);
        response.set_content_type("text/plain");
        response.set_body("400 Bad Request\n");
        return response;
    }

    if (url_path == "/") url_path = "/index.html";

    std::string file_path = root_ + url_path;

    DWORD attrs = GetFileAttributesA(file_path.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        HttpResponse response(404);
        response.set_content_type("text/plain");
        response.set_body("404 Not Found\n");
        return response;
    }
    if (attrs & FILE_ATTRIBUTE_DIRECTORY) {
        file_path += "/index.html";
        attrs = GetFileAttributesA(file_path.c_str());
        if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
            HttpResponse response(404);
            response.set_content_type("text/plain");
            response.set_body("404 Not Found\n");
            return response;
        }
    }

    std::ifstream file(file_path.c_str(), std::ios::binary);
    if (!file) {
        HttpResponse response(403);
        response.set_content_type("text/plain");
        response.set_body("403 Forbidden\n");
        return response;
    }

    std::ostringstream content;
    content << file.rdbuf();
    std::string body = content.str();

    HttpResponse response(200);
    response.set_content_type(mime_type(file_path));
    if (request.method() == "HEAD") body.clear();
    response.set_body(body);
    return response;
}

bool StaticFileHandler::is_unsafe_path(const std::string& path) const {
    if (path.empty() || path[0] != '/') return true;
    if (path.find("..") != std::string::npos) return true;
    if (path.find('\\') != std::string::npos) return true;
    if (path.find('\0') != std::string::npos) return true;
    return false;
}

std::string StaticFileHandler::mime_type(const std::string& path) const {
    size_t dot = path.find_last_of('.');
    std::string ext = dot == std::string::npos ? "" : path.substr(dot);

    if (ext == ".html" || ext == ".htm") return "text/html; charset=utf-8";
    if (ext == ".css") return "text/css; charset=utf-8";
    if (ext == ".js")  return "application/javascript; charset=utf-8";
    if (ext == ".json") return "application/json";
    if (ext == ".txt") return "text/plain; charset=utf-8";
    if (ext == ".png") return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".gif") return "image/gif";
    if (ext == ".svg") return "image/svg+xml";
    if (ext == ".ico") return "image/x-icon";
    return "application/octet-stream";
}

} // namespace sws
