#pragma once
#include <Windows.h>
#include <string>
#include <functional>
#include <vector>
#include <cstdint>

namespace pipe_server
{
    enum class MessageType
    {
        Execute = 0x01,
        Ping = 0x02,
        Shutdown = 0xFF
    };

    struct MessageHeader
    {
        uint32_t type;
        uint32_t length;
        uint32_t checksum;
    };

    using ExecuteHandler = std::function<void(const std::string& script)>;
    using ShutdownHandler = std::function<void()>;

    bool Start(const std::string& name = "ElainaExecutor");
    void Stop();
    bool IsRunning();

    void OnExecute(ExecuteHandler handler);
    void OnShutdown(ShutdownHandler handler);
    bool SendResponse(bool success, const std::string& data = "");

    namespace detail
    {
        DWORD WINAPI PipeThread(LPVOID param);
        bool ProcessMessage(const MessageHeader& hdr, const std::vector<uint8_t>& data);
    }
}
