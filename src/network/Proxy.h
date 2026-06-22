#pragma once
#include <string>
#include <functional>

class Proxy {
public:
    using RequestCallback = std::function<std::string(const std::string& url, const std::string& method, const std::string& body, const std::string& contentType)>;

    static std::string Request(const std::string& url,
                               const std::string& method = "GET",
                               const std::string& body = {},
                               const std::string& contentType = "application/json");

    static void OnRequest(RequestCallback cb);
    static std::string Get(const std::string& url);
    static std::string Post(const std::string& url, const std::string& body);

private:
    static inline RequestCallback s_callback;
};
