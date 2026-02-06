#include "core/Log.h"
#include <cstdio>
#include <ctime>

namespace mt {

static const char* levelStr(LogLevel level) {
    switch (level) {
        case LogLevel::Trace: return "TRACE";
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO ";
        case LogLevel::Warn:  return "WARN ";
        case LogLevel::Error: return "ERROR";
        case LogLevel::Fatal: return "FATAL";
    }
    return "?????";
}

void Logger::log(LogLevel level, std::string_view category, std::string_view message) {
    if (level < min_level_) return;

    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    struct tm tm_buf{};
    localtime_r(&time_t_now, &tm_buf);

    char time_str[32];
    std::snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d.%03d",
                  tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec,
                  static_cast<int>(ms.count()));

    if (node_tag_.empty()) {
        std::fprintf(stderr, "[%s] %s [%.*s] %.*s\n",
                     time_str, levelStr(level),
                     static_cast<int>(category.size()), category.data(),
                     static_cast<int>(message.size()), message.data());
    } else {
        std::fprintf(stderr, "[%s] %s [%s] [%.*s] %.*s\n",
                     time_str, levelStr(level), node_tag_.c_str(),
                     static_cast<int>(category.size()), category.data(),
                     static_cast<int>(message.size()), message.data());
    }
}

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

} // namespace mt
