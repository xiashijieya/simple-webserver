#include "logger.h"

#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <mutex>
#include <sstream>
#include <thread>

namespace sws {

namespace {

LogLevel g_level = log_info;
std::mutex g_mutex;

const char* level_tag(LogLevel level) {
    static const char* k_tags[] = {"DEBUG", "INFO ", "WARN ", "ERROR"};
    return k_tags[level];
}

const char* base_name(const char* path) {
    const char* slash = path;
    for (const char* p = path; *p; ++p) {
        if (*p == '/' || *p == '\\') slash = p + 1;
    }
    return slash;
}

} // namespace

void set_log_level(LogLevel level) {
    g_level = level;
}

void log_message(LogLevel level, const char* file, int line, const char* fmt, ...) {
    if (level < g_level) return;

    char body[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(body, sizeof(body), fmt, args);
    va_end(args);

    std::time_t now = std::time(NULL);
    std::tm local;
    localtime_s(&local, &now);
    char time_text[32];
    strftime(time_text, sizeof(time_text), "%Y-%m-%d %H:%M:%S", &local);

    std::ostringstream tid;
    tid << std::this_thread::get_id();

    std::lock_guard<std::mutex> lock(g_mutex);
    std::fprintf(stderr, "%s [%s] [tid %s] %s:%d  %s\n",
                 time_text, level_tag(level), tid.str().c_str(),
                 base_name(file), line, body);
}

} // namespace sws
