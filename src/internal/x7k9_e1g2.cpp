#include "internal/x7k9_core.hpp"

#include <Windows.h>
#include <WinInet.h>

#include <sstream>

#pragma comment(lib, "wininet.lib")

namespace x7k2::q9m4 {
namespace {

std::string build_header_block(const std::map<std::string, std::string>& headers) {
    std::ostringstream out;
    for (const auto& [key, value] : headers) {
        out << key << ": " << value << "\r\n";
    }
    return out.str();
}

X7H6 read_response(HINTERNET request) {
    X7H6 response;
    DWORD status_code = 0;
    DWORD status_size = sizeof(status_code);
    HttpQueryInfoA(
        request,
        HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
        &status_code,
        &status_size,
        nullptr);
    response.status = static_cast<int>(status_code);

    std::vector<uint8_t> body;
    for (;;) {
        DWORD available = 0;
        if (!InternetQueryDataAvailable(request, &available, 0, 0)) {
            response.error = "InternetQueryDataAvailable failed";
            break;
        }
        if (available == 0) {
            break;
        }
        const size_t offset = body.size();
        body.resize(offset + available);
        DWORD read = 0;
        if (!InternetReadFile(request, body.data() + offset, available, &read)) {
            response.error = "InternetReadFile failed";
            body.resize(offset);
            break;
        }
        body.resize(offset + read);
        if (read == 0) {
            break;
        }
    }
    response.body = std::move(body);
    return response;
}

X7H6 send_request(
    const std::string& verb,
    const std::string& host,
    const std::string& path,
    const std::vector<uint8_t>& body,
    const std::map<std::string, std::string>& headers,
    bool use_https) {
    X7H6 response;
    static const char kAgent[] =
        "Mozilla/5.0 (Macintosh; Intel Mac OS X) AdobeAIR/32.0";

    HINTERNET internet = InternetOpenA(kAgent, INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
    if (!internet) {
        response.error = "InternetOpen failed";
        return response;
    }

    const INTERNET_PORT port = use_https ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;
    HINTERNET connection = InternetConnectA(
        internet,
        host.c_str(),
        port,
        nullptr,
        nullptr,
        INTERNET_SERVICE_HTTP,
        0,
        0);
    if (!connection) {
        InternetCloseHandle(internet);
        response.error = "InternetConnect failed";
        return response;
    }

    DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_KEEP_CONNECTION;
    if (use_https) {
        flags |= INTERNET_FLAG_SECURE;
    }

    HINTERNET request = HttpOpenRequestA(
        connection,
        verb.c_str(),
        path.c_str(),
        nullptr,
        nullptr,
        nullptr,
        flags,
        0);
    if (!request) {
        InternetCloseHandle(connection);
        InternetCloseHandle(internet);
        response.error = "HttpOpenRequest failed";
        return response;
    }

    const std::string header_block = build_header_block(headers);
    const BOOL ok = HttpSendRequestA(
        request,
        header_block.empty() ? nullptr : header_block.c_str(),
        static_cast<DWORD>(header_block.size()),
        body.empty() ? nullptr : const_cast<uint8_t*>(body.data()),
        static_cast<DWORD>(body.size()));
    if (!ok) {
        InternetCloseHandle(request);
        InternetCloseHandle(connection);
        InternetCloseHandle(internet);
        response.error = "HttpSendRequest failed";
        return response;
    }

    response = read_response(request);
    InternetCloseHandle(request);
    InternetCloseHandle(connection);
    InternetCloseHandle(internet);
    return response;
}

}  // namespace

X7H6 X7C7::post(
    const std::string& host,
    const std::string& path,
    const std::vector<uint8_t>& body,
    const std::map<std::string, std::string>& headers,
    bool use_https) {
    return send_request("POST", host, path, body, headers, use_https);
}

X7H6 X7C7::get(
    const std::string& host,
    const std::string& path,
    const std::map<std::string, std::string>& headers,
    bool use_https) {
    return send_request("GET", host, path, {}, headers, use_https);
}

}  // namespace x7k2::q9m4
