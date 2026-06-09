#include "internal/x7k9_core.hpp"

#include <Windows.h>
#include <WinInet.h>

#include <cstring>
#include <string>

#pragma comment(lib, "wininet.lib")

namespace x7k2::q9m4 {
namespace {

static HINTERNET g_internet = nullptr;
static HINTERNET g_connect = nullptr;
static std::string g_connected_host;

static const char kAgent[] =
    "Mozilla/5.0 (Windows; U; fr-FR) AppleWebKit/533.19.4 (KHTML, like Gecko) AdobeAIR/32.0";

static const char kReferer[] =
    "app:/cache/t1.bin/[[DYNAMIC]]/2/[[DYNAMIC]]/3";

static const char kSendHeaders[] =
    "x-flash-version: 32,0,0,100\r\n"
    "Content-Type: application/x-amf\r\n";

static const char kAcceptHeader[] =
    "Accept: text/xml, application/xml, application/xhtml+xml, text/html;q=0.9, "
    "text/plain;q=0.8, text/css, image/png, image/jpeg, image/gif;q=0.8, "
    "application/x-shockwave-flash, video/mp4;q=0.9, flv-application/octet-stream;q=0.8, "
    "video/x-flv;q=0.7, audio/mp4, application/futuresplash, */*;q=0.5, application/x-mpegURL";

std::string wininet_error(DWORD code) {
    if (code == 0) {
        return "0";
    }
    char* message = nullptr;
    const DWORD len = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_FROM_SYSTEM,
        GetModuleHandleA("wininet.dll"),
        code,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPSTR>(&message),
        0,
        nullptr);
    if (len == 0 || message == nullptr) {
        return std::to_string(code);
    }
    std::string out(message, len);
    LocalFree(message);
    while (!out.empty() && (out.back() == '\r' || out.back() == '\n')) {
        out.pop_back();
    }
    return std::to_string(code) + " " + out;
}

bool ensure_handles(const char* host) {
    if (!g_internet) {
        g_internet = InternetOpenA(kAgent, INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
        if (!g_internet) {
            return false;
        }
        InternetSetOptionA(
            g_internet,
            INTERNET_OPTION_USER_AGENT,
            const_cast<char*>(kAgent),
            static_cast<DWORD>(sizeof(kAgent)));
    }

    if (!g_connect || g_connected_host != host) {
        if (g_connect) {
            InternetCloseHandle(g_connect);
            g_connect = nullptr;
        }
        g_connect = InternetConnectA(
            g_internet,
            host,
            INTERNET_DEFAULT_HTTPS_PORT,
            nullptr,
            nullptr,
            INTERNET_SERVICE_HTTP,
            0,
            0);
        if (!g_connect) {
            return false;
        }
        g_connected_host = host;
    }
    return true;
}

X7N8 read_http_response(HINTERNET request) {
    X7N8 result;
    DWORD status_code = 0;
    DWORD status_size = sizeof(status_code);
    HttpQueryInfoA(
        request,
        HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
        &status_code,
        &status_size,
        nullptr);
    result.http_status = static_cast<int>(status_code);

    int32_t content_length = 0;
    DWORD length_size = sizeof(content_length);
    const BOOL has_length = HttpQueryInfoA(
        request,
        HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER,
        &content_length,
        &length_size,
        nullptr);

    if (has_length && content_length > 0) {
        result.body.resize(static_cast<size_t>(content_length));
        DWORD total_read = 0;
        while (total_read < static_cast<DWORD>(content_length)) {
            DWORD read = 0;
            if (!InternetReadFile(
                    request,
                    result.body.data() + total_read,
                    static_cast<DWORD>(content_length) - total_read,
                    &read)) {
                result.error = "InternetReadFile failed (" + wininet_error(GetLastError()) + ")";
                result.body.clear();
                return result;
            }
            if (read == 0) {
                break;
            }
            total_read += read;
        }
        result.body.resize(total_read);
        return result;
    }

    std::vector<uint8_t> buffer(8192);
    for (;;) {
        DWORD read = 0;
        if (!InternetReadFile(request, buffer.data(), static_cast<DWORD>(buffer.size()), &read)) {
            result.error = "InternetReadFile failed (" + wininet_error(GetLastError()) + ")";
            result.body.clear();
            return result;
        }
        if (read == 0) {
            break;
        }
        result.body.insert(result.body.end(), buffer.begin(), buffer.begin() + read);
    }
    return result;
}

}  // namespace

X7N8 x7k9_n2c4(const char* host, const char* path, const uint8_t* data, int32_t length) {
    X7N8 result;
    if (!ensure_handles(host)) {
        result.error = "WinInet connect failed (" + wininet_error(GetLastError()) + ")";
        return result;
    }

    HINTERNET request = HttpOpenRequestA(
        g_connect,
        "POST",
        path,
        nullptr,
        kReferer,
        nullptr,
        INTERNET_FLAG_KEEP_CONNECTION | INTERNET_FLAG_RELOAD | INTERNET_FLAG_RESYNCHRONIZE |
            INTERNET_FLAG_SECURE,
        0);
    if (!request) {
        result.error = "HttpOpenRequest failed (" + wininet_error(GetLastError()) + ")";
        return result;
    }

    BOOL http_decoding = TRUE;
    InternetSetOption(request, INTERNET_OPTION_HTTP_DECODING, &http_decoding, sizeof(http_decoding));

    if (!HttpAddRequestHeadersA(
            request,
            kAcceptHeader,
            static_cast<DWORD>(-1),
            HTTP_ADDREQ_FLAG_ADD | HTTP_ADDREQ_FLAG_COALESCE)) {
        InternetCloseHandle(request);
        result.error = "HttpAddRequestHeaders failed (" + wininet_error(GetLastError()) + ")";
        return result;
    }

    if (!HttpSendRequestA(
            request,
            kSendHeaders,
            static_cast<DWORD>(strlen(kSendHeaders)),
            length > 0 ? const_cast<uint8_t*>(data) : nullptr,
            static_cast<DWORD>(length))) {
        InternetCloseHandle(request);
        result.error = "HttpSendRequest failed (" + wininet_error(GetLastError()) + ")";
        return result;
    }

    result = read_http_response(request);
    if (!result.error.empty()) {
        InternetCloseHandle(request);
        return result;
    }
    result.body = x7k9_z1d3(std::move(result.body));
    InternetCloseHandle(request);
    return result;
}

void x7k9_n2s0() {
    if (g_connect) {
        InternetCloseHandle(g_connect);
        g_connect = nullptr;
    }
    if (g_internet) {
        InternetCloseHandle(g_internet);
        g_internet = nullptr;
    }
    g_connected_host.clear();
}

}  // namespace x7k2::q9m4
