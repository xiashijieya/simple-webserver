#ifndef SWS_LOGGER_H
#define SWS_LOGGER_H

namespace sws {

enum LogLevel {
    log_debug,
    log_info,
    log_warn,
    log_error
};

void set_log_level(LogLevel level);
void log_message(LogLevel level, const char* file, int line, const char* fmt, ...);

} // namespace sws

#define LOG_DEBUG(...) ::sws::log_message(::sws::log_debug, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...)  ::sws::log_message(::sws::log_info,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(...)  ::sws::log_message(::sws::log_warn,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...) ::sws::log_message(::sws::log_error, __FILE__, __LINE__, __VA_ARGS__)

#endif
