#pragma once
#include <string>
#include <functional>
#include <memory>

class Session;

// Layer 3: wraps HTTP server, routes requests to handlers
class NetworkService {
public:
    using RequestHandler = std::function<std::string(const std::string& method, const std::string& path, const std::string& body)>;

    NetworkService();
    ~NetworkService();

    bool Start(int port = 19283);
    void Stop();
    void SetHandler(RequestHandler handler);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
