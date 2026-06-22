#pragma once
#include <string>
#include <variant>

enum class Err {
    None = 0,
    SyscallInitFailed,
    ProcessNotFound,
    ProcessOpenFailed,
    MemoryReadFailed,
    MemoryWriteFailed,
    AllocFailed,
    DataModelNotFound,
    CoreGuiNotFound,
    ModulesNotFound,
    NoUnloadedModule,
    CompileFailed,
    EncodeFailed,
    SpoofFailed,
    InjectFailed,
    Timeout,
    InvalidState,
    ServerFailed,
    Unknown
};

inline std::string_view ErrStr(Err e) {
    switch (e) {
        case Err::None: return "OK";
        case Err::SyscallInitFailed: return "Syscall init failed";
        case Err::ProcessNotFound: return "Process not found";
        case Err::ProcessOpenFailed: return "Failed to open process";
        case Err::MemoryReadFailed: return "Memory read failed";
        case Err::MemoryWriteFailed: return "Memory write failed";
        case Err::AllocFailed: return "Remote allocation failed";
        case Err::DataModelNotFound: return "DataModel not found";
        case Err::CoreGuiNotFound: return "CoreGui not found";
        case Err::ModulesNotFound: return "Modules not found";
        case Err::NoUnloadedModule: return "No unloaded module found";
        case Err::CompileFailed: return "Luau compile failed";
        case Err::EncodeFailed: return "RSB1 encode failed";
        case Err::SpoofFailed: return "Spoof target write failed";
        case Err::InjectFailed: return "Inject failed";
        case Err::Timeout: return "Operation timed out";
        case Err::InvalidState: return "Invalid state";
        case Err::ServerFailed: return "HTTP server failed";
        case Err::Unknown: return "Unknown error";
    }
    return "Unknown";
}

template<typename T>
struct Result {
    T value;
    Err error = Err::None;
    std::string message;

    Result() : value{}, error(Err::None) {}
    Result(T v) : value(std::move(v)), error(Err::None) {}
    Result(Err e, std::string msg = {}) : value{}, error(e), message(std::move(msg)) {}

    bool Ok() const { return error == Err::None; }
    explicit operator bool() const { return Ok(); }
    std::string ErrorMsg() const {
        if (message.empty()) return std::string(ErrStr(error));
        return std::string(ErrStr(error)) + ": " + message;
    }
};

template<>
struct Result<void> {
    Err error = Err::None;
    std::string message;

    Result() : error(Err::None) {}
    Result(Err e, std::string msg = {}) : error(e), message(std::move(msg)) {}

    bool Ok() const { return error == Err::None; }
    explicit operator bool() const { return Ok(); }
    std::string ErrorMsg() const {
        if (message.empty()) return std::string(ErrStr(error));
        return std::string(ErrStr(error)) + ": " + message;
    }
};
