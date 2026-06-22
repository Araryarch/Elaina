#include "Log.h"
#include <cstdarg>
#include <chrono>

static const char* LevelName(LogLevel level) {
    switch (level) {
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return " INFO";
        case LogLevel::Warn:  return " WARN";
        case LogLevel::Error: return "ERROR";
    }
    return "UNKN";
}

static std::string Timestamp() {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    struct tm buf;
    localtime_s(&buf, &t);
    char ts[24];
    strftime(ts, sizeof(ts), "%H:%M:%S", &buf);
    snprintf(ts + 8, 16, ".%03lld", (long long)ms.count());
    return ts;
}

void Log::VLog(LogLevel level, const char* fmt, va_list args) {
    char buf[4096];
    vsnprintf(buf, sizeof(buf), fmt, args);
    Write(level, buf);
}

void Log::Write(LogLevel level, std::string_view msg) {
    if (level < sLevel) return;

    std::lock_guard lock(sMutex);
    auto ts = Timestamp();
    auto line = std::string("[" + ts + "][" + LevelName(level) + "] " + std::string(msg));

    if (sFile) {
        fprintf(sFile, "%s\n", line.c_str());
        fflush(sFile);
    }

    if (sSink) {
        sSink(level, line);
    }

    HANDLE h = GetStdHandle(level >= LogLevel::Error ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE);
    if (h && h != INVALID_HANDLE_VALUE) {
        WORD attr = level >= LogLevel::Error ? FOREGROUND_RED :
                    level == LogLevel::Warn  ? FOREGROUND_RED | FOREGROUND_GREEN :
                    level == LogLevel::Debug ? FOREGROUND_GREEN | FOREGROUND_BLUE :
                                               FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
        SetConsoleTextAttribute(h, attr);
        DWORD written;
        WriteConsoleA(h, line.c_str(), (DWORD)line.size(), &written, nullptr);
        WriteConsoleA(h, "\n", 1, &written, nullptr);
        SetConsoleTextAttribute(h, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
    }
}

void Log::SetFileSink(const char* path) {
    std::lock_guard lock(sMutex);
    if (sFile) fclose(sFile);
    fopen_s(&sFile, path, "a");
}

void Log::Debug(const char* fmt, ...) { va_list args; va_start(args, fmt); VLog(LogLevel::Debug, fmt, args); va_end(args); }
void Log::Info(const char* fmt, ...)  { va_list args; va_start(args, fmt); VLog(LogLevel::Info, fmt, args); va_end(args); }
void Log::Warn(const char* fmt, ...)  { va_list args; va_start(args, fmt); VLog(LogLevel::Warn, fmt, args); va_end(args); }
void Log::Error(const char* fmt, ...) { va_list args; va_start(args, fmt); VLog(LogLevel::Error, fmt, args); va_end(args); }
