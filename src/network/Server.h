#pragma once
#include <string>
#include <functional>
#include <thread>
#include <memory>
#include "core/Result.h"

class Server {
public:
    using RequestHandler = std::function<std::string(const std::string& method, const std::string& path, const std::string& body)>;

    Server();
    ~Server();

    bool Start(int port = 19283);
    void Stop();
    void OnRequest(RequestHandler handler);

    bool IsRunning() const;

private:
    void Worker();

    std::unique_ptr<std::thread> m_thread;
    RequestHandler m_handler;
    bool m_running{false};
    int m_port{19283};
};
