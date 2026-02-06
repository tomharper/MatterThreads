#pragma once

#include <string>
#include <string_view>
#include <cstdio>
#include <chrono>

namespace mt {

enum class LogLevel : uint8_t {
    Trace = 0,
    Debug = 1,
    Info  = 2,
    Warn  = 3,
    Error = 4,
    Fatal = 5
};

class Logger {
    LogLevel min_level_ = LogLevel::Info;
    std::string node_tag_;

public:
    void setLevel(LogLevel level) { min_level_ = level; }
    LogLevel level() const { return min_level_; }

    void setNodeTag(std::string_view tag) { node_tag_ = std::string(tag); }
    const std::string& nodeTag() const { return node_tag_; }

    void log(LogLevel level, std::string_view category, std::string_view message);

    static Logger& instance();
};

// Convenience macros
#define MT_LOG(level, cat, msg) \
    do { ::mt::Logger::instance().log(level, cat, msg); } while(0)

#define MT_TRACE(cat, msg) MT_LOG(::mt::LogLevel::Trace, cat, msg)
#define MT_DEBUG(cat, msg) MT_LOG(::mt::LogLevel::Debug, cat, msg)
#define MT_INFO(cat, msg)  MT_LOG(::mt::LogLevel::Info, cat, msg)
#define MT_WARN(cat, msg)  MT_LOG(::mt::LogLevel::Warn, cat, msg)
#define MT_ERROR(cat, msg) MT_LOG(::mt::LogLevel::Error, cat, msg)

} // namespace mt
