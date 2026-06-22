#include "Proxy.h"
#include "core/Log.h"
#include <winhttp.h>
#include <string>
#include <vector>
#include <sstream>

#pragma comment(lib, "winhttp.lib")

void Proxy::OnRequest(RequestCallback cb) {
    s_callback = std::move(cb);
}

std::string Proxy::Get(const std::string& url) { return Request(url, "GET"); }
std::string Proxy::Post(const std::string& url, const std::string& body) { return Request(url, "POST", body); }

std::string Proxy::Request(const std::string& url,
                           const std::string& method,
                           const std::string& body,
                           const std::string& contentType) {
    if (s_callback) {
        return s_callback(url, method, body, contentType);
    }

    std::string host, path;
    bool is_https = url.find("https://") == 0;

    auto strip = url.find("://");
    if (strip == std::string::npos) return {};

    auto slash = url.find('/', strip + 3);
    if (slash == std::string::npos) {
        host = url.substr(strip + 3);
        path = "/";
    } else {
        host = url.substr(strip + 3, slash - strip - 3);
        path = url.substr(slash);
    }

    if (path.empty()) path = "/";

    HINTERNET session = WinHttpOpen(L"Elaina/1.0",
                                    WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                    nullptr, nullptr, 0);
    if (!session) return {};

    std::wstring whost(host.begin(), host.end());
    HINTERNET connect = WinHttpConnect(session, whost.c_str(),
                                        is_https ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT, 0);
    if (!connect) { WinHttpCloseHandle(session); return {}; }

    std::wstring wmethod(method.begin(), method.end());
    std::wstring wpath(path.begin(), path.end());
    HINTERNET request = WinHttpOpenRequest(connect, wmethod.c_str(), wpath.c_str(),
                                            nullptr, nullptr, nullptr,
                                            is_https ? WINHTTP_FLAG_SECURE : 0);
    if (!request) { WinHttpCloseHandle(connect); WinHttpCloseHandle(session); return {}; }

    LPCWSTR types[] = { L"*/*", nullptr };
    WinHttpSetOption(request, WINHTTP_OPTION_USER_AGENT, (LPVOID)L"Elaina/1.0", sizeof(L"Elaina/1.0"));

    if (!body.empty()) {
        std::wstring wct(contentType.begin(), contentType.end());
        WinHttpAddRequestHeaders(request, (L"Content-Type: " + wct).c_str(), -1, WINHTTP_ADDREQ_FLAG_ADD);
    }

    bool sent = WinHttpSendRequest(request,
                                    WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                    body.empty() ? WINHTTP_NO_REQUEST_DATA : (LPVOID)body.data(),
                                    body.empty() ? 0 : (DWORD)body.size(),
                                    body.empty() ? 0 : (DWORD)body.size(), 0);
    if (!sent) { WinHttpCloseHandle(request); WinHttpCloseHandle(connect); WinHttpCloseHandle(session); return {}; }

    WinHttpReceiveResponse(request, nullptr);

    std::string result;
    DWORD size = 0;
    do {
        if (!WinHttpQueryDataAvailable(request, &size)) break;
        if (size == 0) break;
        std::vector<char> buf(size + 1);
        DWORD read = 0;
        if (WinHttpReadData(request, buf.data(), size, &read))
            result.append(buf.data(), read);
    } while (size > 0);

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    return result;
}
