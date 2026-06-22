#pragma once
#include <string>
#include <map>
#include <vector>
#include <windows.h>
#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")

struct WinHttpResponse {
    int status_code = 0;
    std::string body;
    std::map<std::string, std::string> headers;
};

class WinHttpClient {
public:
    static WinHttpResponse MakeRequest(const std::string& method, const std::string& urlStr, const std::map<std::string, std::string>& headers, const std::string& body) {
        WinHttpResponse response;

        std::wstring wUrl = s2ws(urlStr);
        URL_COMPONENTS urlComp = { 0 };
        urlComp.dwStructSize = sizeof(urlComp);

        wchar_t hostName[256] = {0};
        wchar_t urlPath[1024] = {0};

        urlComp.lpszHostName = hostName;
        urlComp.dwHostNameLength = 256;
        urlComp.lpszUrlPath = urlPath;
        urlComp.dwUrlPathLength = 1024;

        if (!WinHttpCrackUrl(wUrl.c_str(), (DWORD)wUrl.length(), 0, &urlComp)) return response;

        bool isHttps = (urlComp.nScheme == INTERNET_SCHEME_HTTPS);

        HINTERNET hSession = WinHttpOpen(L"Elaina/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) return response;

        HINTERNET hConnect = WinHttpConnect(hSession, hostName, urlComp.nPort, 0);
        if (hConnect) {
            std::wstring wMethod = s2ws(method);
            HINTERNET hRequest = WinHttpOpenRequest(hConnect, wMethod.c_str(), urlPath, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, isHttps ? WINHTTP_FLAG_SECURE : 0);

            if (hRequest) {
                DWORD flags = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_CN_INVALID | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;
                WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &flags, sizeof(flags));

                std::wstring wHeaders;
                for (const auto& kv : headers) {
                    wHeaders += s2ws(kv.first + ": " + kv.second + "\r\n");
                }

                bool bResults = WinHttpSendRequest(hRequest,
                    wHeaders.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : wHeaders.c_str(),
                    wHeaders.empty() ? 0 : (DWORD)-1,
                    (LPVOID)(body.empty() ? NULL : body.data()),
                    (DWORD)body.size(), (DWORD)body.size(), 0);

                if (bResults) bResults = WinHttpReceiveResponse(hRequest, NULL);

                if (bResults) {
                    DWORD dwStatusCode = 0;
                    DWORD dwSize = sizeof(dwStatusCode);
                    if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &dwStatusCode, &dwSize, WINHTTP_NO_HEADER_INDEX)) {
                        response.status_code = dwStatusCode;
                    }

                    DWORD dwAvailable = 0;
                    do {
                        if (!WinHttpQueryDataAvailable(hRequest, &dwAvailable)) break;
                        if (dwAvailable == 0) break;
                        std::vector<char> buffer(dwAvailable);
                        DWORD dwDownloaded = 0;
                        if (!WinHttpReadData(hRequest, buffer.data(), dwAvailable, &dwDownloaded)) break;
                        response.body.append(buffer.data(), dwDownloaded);
                    } while (dwAvailable > 0);
                }
                WinHttpCloseHandle(hRequest);
            }
            WinHttpCloseHandle(hConnect);
        }
        WinHttpCloseHandle(hSession);

        return response;
    }

private:
    static std::wstring s2ws(const std::string& s) {
        if (s.empty()) return std::wstring();
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, &s[0], (int)s.size(), NULL, 0);
        std::wstring wstrTo(size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, &s[0], (int)s.size(), &wstrTo[0], size_needed);
        return wstrTo;
    }
};
