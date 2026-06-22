#pragma once
#include <string>
#include <string_view>
#include <cstdio>
#include <ctime>
#include <mutex>
#include <functional>

enum class LogLevel {
    Debug = 0,
    Info = 1,
    Warn = 2,
    Error = 3
};

class Log {
public:
    using SinkFn = std::function<void(LogLevel, std::string_view)>;

    static void SetLevel(LogLevel level) { sLevel = level; }
    static LogLevel GetLevel() { return sLevel; }

    static void SetSink(SinkFn fn) { sSink = std::move(fn); }

    static void SetFileSink(const char* path);

    static void Debug(const char* fmt, ...);
    static void Info(const char* fmt, ...);
    static void Warn(const char* fmt, ...);
    static void Error(const char* fmt, ...);

private:
    static void VLog(LogLevel level, const char* fmt, va_list args);
    static void Write(LogLevel level, std::string_view msg);

    static inline LogLevel sLevel = LogLevel::Info;
    static inline SinkFn sSink = nullptr;
    static inline std::mutex sMutex;
    static inline FILE* sFile = nullptr;
};
