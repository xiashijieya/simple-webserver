#ifndef SWS_MINI_TEST_H
#define SWS_MINI_TEST_H

#include <sstream>
#include <string>
#include <vector>

namespace sws_test {

typedef void (*test_fn)();

struct TestCase {
    const char* name;
    test_fn fn;
};

std::vector<TestCase>& test_registry();
bool register_test(const char* name, test_fn fn);
int run_all(int argc, char** argv);

void record_check(bool passed, const std::string& expr,
                  const char* file, int line);

template <typename Actual, typename Expected>
void check_eq(const Actual& actual, const Expected& expected,
              const char* actual_text, const char* expected_text,
              const char* file, int line) {
    bool passed = actual == expected;
    std::ostringstream message;
    message << actual_text << " == " << expected_text
            << "  (actual: " << actual << ", expected: " << expected << ")";
    record_check(passed, message.str(), file, line);
}

} // namespace sws_test

#define TEST_CASE(name)                                                     \
    static void name();                                                     \
    static const bool name##_registered =                                  \
        ::sws_test::register_test(#name, &name);                           \
    static void name()

#define CHECK(cond)                                                         \
    ::sws_test::record_check(static_cast<bool>(cond), #cond,               \
                             __FILE__, __LINE__)

#define CHECK_EQ(actual, expected)                                          \
    ::sws_test::check_eq(actual, expected, #actual, #expected,             \
                         __FILE__, __LINE__)

#endif
