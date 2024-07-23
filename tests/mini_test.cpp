#include "mini_test.h"

#include <cstdio>
#include <cstring>

namespace sws_test {

namespace {

int g_failures = 0;
const char* g_current_case = "";

}

std::vector<TestCase>& test_registry() {
    static std::vector<TestCase> registry;
    return registry;
}

bool register_test(const char* name, test_fn fn) {
    TestCase test_case;
    test_case.name = name;
    test_case.fn = fn;
    test_registry().push_back(test_case);
    return true;
}

void record_check(bool passed, const std::string& expr,
                  const char* file, int line) {
    if (passed) return;

    ++g_failures;
    std::printf("    [FAIL] %s\n           %s:%d  %s\n",
                g_current_case, file, line, expr.c_str());
}

int run_all(int argc, char** argv) {
    const char* filter = argc >= 2 ? argv[1] : NULL;
    int executed = 0;
    int failed_cases = 0;

    for (size_t i = 0; i < test_registry().size(); ++i) {
        const TestCase& test_case = test_registry()[i];
        if (filter && std::strstr(test_case.name, filter) == NULL) continue;

        g_current_case = test_case.name;
        int failures_before = g_failures;
        test_case.fn();
        ++executed;

        if (g_failures == failures_before) {
            std::printf("[PASS] %s\n", test_case.name);
        } else {
            ++failed_cases;
            std::printf("[FAIL] %s\n", test_case.name);
        }
    }

    std::printf("\n%d cases executed, %d cases failed, %d checks failed\n",
                executed, failed_cases, g_failures);
    return g_failures == 0 ? 0 : 1;
}

} // namespace sws_test

int main(int argc, char** argv) {
    return ::sws_test::run_all(argc, argv);
}
