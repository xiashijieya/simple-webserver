#ifndef SWS_BUFFER_H
#define SWS_BUFFER_H

#include <winsock2.h>

#include <cstddef>
#include <string>
#include <vector>

namespace sws {

class Buffer {
public:
    static const size_t k_initial_size = 1024;

    explicit Buffer(size_t initial_size = k_initial_size);

    size_t readable_bytes() const { return write_idx_ - read_idx_; }
    size_t writable_bytes() const { return data_.size() - write_idx_; }

    const char* peek() const { return begin() + read_idx_; }
    char* begin_write() { return begin() + write_idx_; }
    const char* begin_write() const { return begin() + write_idx_; }
    void has_written(size_t len) { write_idx_ += len; }

    void ensure_writable(size_t len);
    void append(const char* data, size_t len);
    void append(const std::string& data);

    void retrieve(size_t len);
    void retrieve_all();
    std::string retrieve_as_string(size_t len);
    std::string retrieve_all_as_string();

    const char* find_crlf() const;

    int read_fd(SOCKET fd);

private:
    char* begin() { return &*data_.begin(); }
    const char* begin() const { return &*data_.begin(); }
    void make_space(size_t len);

    std::vector<char> data_;
    size_t read_idx_;
    size_t write_idx_;
};

} // namespace sws

#endif
