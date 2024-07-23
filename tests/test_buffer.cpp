#include <string>

#include "buffer.h"
#include "mini_test.h"

using namespace sws;

TEST_CASE(buffer_append_and_retrieve) {
    Buffer buf(4);
    CHECK_EQ(buf.readable_bytes(), static_cast<size_t>(0));

    buf.append("hello", 5);
    CHECK_EQ(buf.readable_bytes(), static_cast<size_t>(5));

    std::string part = buf.retrieve_as_string(2);
    CHECK_EQ(part, std::string("he"));
    CHECK_EQ(buf.readable_bytes(), static_cast<size_t>(3));
}

TEST_CASE(buffer_grows_when_full) {
    Buffer buf(4);
    buf.append("abcdefgh", 8);
    CHECK_EQ(buf.readable_bytes(), static_cast<size_t>(8));
    CHECK_EQ(buf.retrieve_all_as_string(), std::string("abcdefgh"));
}

TEST_CASE(buffer_reuses_front_space) {
    Buffer buf(8);
    buf.append("abcdefgh", 8);
    buf.retrieve(6);
    buf.append("ij", 2);
    CHECK_EQ(buf.readable_bytes(), static_cast<size_t>(4));
    CHECK_EQ(buf.retrieve_all_as_string(), std::string("ghij"));
}

TEST_CASE(buffer_finds_crlf) {
    Buffer buf;
    CHECK(buf.find_crlf() == NULL);

    buf.append("GET / HTTP/1.1\r\n", 16);
    const char* crlf = buf.find_crlf();
    CHECK(crlf != NULL);
    CHECK_EQ(static_cast<size_t>(crlf - buf.peek()), static_cast<size_t>(14));
}

TEST_CASE(buffer_retrieve_all_resets) {
    Buffer buf;
    buf.append("xyz", 3);
    buf.retrieve_all();
    CHECK_EQ(buf.readable_bytes(), static_cast<size_t>(0));
    buf.append("q", 1);
    CHECK_EQ(buf.retrieve_all_as_string(), std::string("q"));
}
