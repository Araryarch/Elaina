#include "pipe_server.h"
#include <vector>
#include <mutex>
#include <thread>
#include <chrono>

namespace pipe_server
{
    static HANDLE g_pipe = INVALID_HANDLE_VALUE;
    static HANDLE g_thread = nullptr;
    static bool g_running = false;
    static std::string g_pipe_name;

    static ExecuteHandler g_exec_handler;
    static ShutdownHandler g_shutdown_handler;

    static std::mutex g_pipe_mutex;

    bool Start(const std::string& name)
    {
        g_pipe_name = R"(\\.\pipe\)" + name;

        g_running = true;
        g_thread = CreateThread(nullptr, 0, detail::PipeThread, nullptr, 0, nullptr);

        return g_thread != nullptr;
    }

    void Stop()
    {
        g_running = false;

        if (g_pipe != INVALID_HANDLE_VALUE)
        {
            DisconnectNamedPipe(g_pipe);
            CloseHandle(g_pipe);
            g_pipe = INVALID_HANDLE_VALUE;
        }

        if (g_thread)
        {
            WaitForSingleObject(g_thread, 1000);
            CloseHandle(g_thread);
            g_thread = nullptr;
        }
    }

    bool IsRunning()
    {
        return g_running;
    }

    void OnExecute(ExecuteHandler handler)
    {
        g_exec_handler = handler;
    }

    void OnShutdown(ShutdownHandler handler)
    {
        g_shutdown_handler = handler;
    }

    bool SendResponse(bool success, const std::string& data)
    {
        std::lock_guard<std::mutex> lock(g_pipe_mutex);
        if (g_pipe == INVALID_HANDLE_VALUE) return false;

        // Build response: [success:1][data_len:4][data...]
        std::vector<uint8_t> response;
        response.push_back(success ? 1 : 0);
        uint32_t len = (uint32_t)data.size();
        response.insert(response.end(), (uint8_t*)&len, (uint8_t*)&len + 4);
        response.insert(response.end(), data.begin(), data.end());

        DWORD written;
        return WriteFile(g_pipe, response.data(), (DWORD)response.size(),
            &written, nullptr) != FALSE;
    }

    namespace detail
    {
        DWORD WINAPI PipeThread(LPVOID param)
        {
            while (g_running)
            {
                g_pipe = CreateNamedPipeA(
                    g_pipe_name.c_str(),
                    PIPE_ACCESS_DUPLEX,
                    PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                    PIPE_UNLIMITED_INSTANCES,
                    4096, 4096,
                    0, nullptr
                );

                if (g_pipe == INVALID_HANDLE_VALUE)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    continue;
                }

                bool connected = ConnectNamedPipe(g_pipe, nullptr)
                    ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);

                if (!connected)
                {
                    CloseHandle(g_pipe);
                    g_pipe = INVALID_HANDLE_VALUE;
                    continue;
                }

                // Read messages
                while (g_running)
                {
                    MessageHeader hdr;
                    DWORD read;

                    if (!ReadFile(g_pipe, &hdr, sizeof(hdr), &read, nullptr))
                        break;

                    if (read != sizeof(hdr))
                        break;

                    std::vector<uint8_t> msg_data(hdr.length);
                    if (hdr.length > 0)
                    {
                        DWORD data_read = 0;
                        if (!ReadFile(g_pipe, msg_data.data(), hdr.length,
                            &data_read, nullptr))
                            break;
                    }

                    if (!ProcessMessage(hdr, msg_data))
                        break;
                }

                DisconnectNamedPipe(g_pipe);
                CloseHandle(g_pipe);
                g_pipe = INVALID_HANDLE_VALUE;
            }

            return 0;
        }

        bool ProcessMessage(const MessageHeader& hdr, const std::vector<uint8_t>& data)
        {
            switch (static_cast<MessageType>(hdr.type))
            {
            case MessageType::Execute:
            {
                std::string script(data.begin(), data.end());
                if (g_exec_handler)
                    g_exec_handler(script);
                SendResponse(true, "Script executed");
                return true;
            }
            case MessageType::Ping:
                SendResponse(true, "Pong");
                return true;
            case MessageType::Shutdown:
                if (g_shutdown_handler)
                    g_shutdown_handler();
                SendResponse(true, "Shutting down");
                return false;
            default:
                SendResponse(false, "Unknown message type");
                return true;
            }
        }
    }
}
