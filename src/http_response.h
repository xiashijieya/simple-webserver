#ifndef SWS_HTTP_RESPONSE_H
#define SWS_HTTP_RESPONSE_H

#include <map>
#include <string>

namespace sws {

class HttpResponse {
public:
    explicit HttpResponse(int status = 200);

    void set_status(int status);
    void set_header(const std::string& key, const std::string& value);
    void set_content_type(const std::string& content_type);
    void set_body(const std::string& body);

    int status() const { return status_; }
    std::string serialize(bool keep_alive) const;

private:
    const char* status_text() const;

    int status_;
    std::map<std::string, std::string> headers_;
    std::string body_;
};

} // namespace sws

#endif
