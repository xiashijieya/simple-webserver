#include "buffer.h"

#include <cstring>
#include <algorithm>

namespace sws {

Buffer::Buffer(size_t initial_size)
    : data_(initial_size), read_idx_(0), write_idx_(0) {
}

void Buffer::make_space(size_t len) {
    if (writable_bytes() + read_idx_ < len) {
        data_.resize(write_idx_ + len);
        return;
    }
    size_t readable = readable_bytes();
    std::memmove(begin(), peek(), readable);
    read_idx_ = 0;
    write_idx_ = readable;
}

void Buffer::ensure_writable(size_t len) {
    if (writable_bytes() < len) make_space(len);
}

void Buffer::append(const char* data, size_t len) {
    ensure_writable(len);
    std::memcpy(begin_write(), data, len);
    has_written(len);
}

void Buffer::append(const std::string& data) {
    append(data.data(), data.size());
}

void Buffer::retrieve(size_t len) {
    if (len >= readable_bytes()) {
        retrieve_all();
        return;
    }
    read_idx_ += len;
}

void Buffer::retrieve_all() {
    read_idx_ = 0;
    write_idx_ = 0;
}

std::string Buffer::retrieve_as_string(size_t len) {
    size_t n = std::min(len, readable_bytes());
    std::string result(peek(), n);
    retrieve(n);
    return result;
}

std::string Buffer::retrieve_all_as_string() {
    return retrieve_as_string(readable_bytes());
}

const char* Buffer::find_crlf() const {
    const char* end = peek() + readable_bytes();
    const char* found = std::search(peek(), end, "\r\n", "\r\n" + 2);
    return found == end ? NULL : found;
}

int Buffer::read_fd(SOCKET fd) {
    char extra[65536];
    int n = recv(fd, extra, sizeof(extra), 0);
    if (n > 0) append(extra, static_cast<size_t>(n));
    return n;
}

} // namespace sws
