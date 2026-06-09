#include "internal/x7k9_core.hpp"

#include <cctype>
#include <stdexcept>

namespace x7k2::q9m4 {
namespace {

constexpr const char* kClientId = "unity.client";
constexpr const char* kClientSecret = "secret";
constexpr const char* kBasicAuth = "dW5pdHkuY2xpZW50OnNlY3JldA==";
constexpr const char* kMsp1GameId = "5ooi";
constexpr const char* kTokenHost = "eu-secure.mspapis.com";
constexpr const char* kTokenPath = "/loginidentity/connect/token";
constexpr const char* kProfileHost = "eu.mspapis.com";

std::map<std::string, std::string> nebula_browser_headers() {
    return {
        {"Accept", "*/*"},
        {"Accept-Language", "fr-FR,fr;q=0.9"},
        {"Origin", "https://moviestarplanet2.com"},
        {"Referer", "https://moviestarplanet2.com/"},
        {"User-Agent",
         "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) "
         "Chrome/146.0.0.0 Safari/537.36"},
    };
}

std::string decode_jwt_payload_json(const std::string& token) {
    const auto first = token.find('.');
    const auto second = token.find('.', first + 1);
    if (first == std::string::npos || second == std::string::npos) {
        throw std::runtime_error("invalid jwt");
    }
    const auto payload = base64url_decode(token.substr(first + 1, second - first - 1));
    return std::string(payload.begin(), payload.end());
}

std::string form_body(const std::map<std::string, std::string>& fields) {
    std::string out;
    bool first = true;
    for (const auto& [key, value] : fields) {
        if (!first) {
            out.push_back('&');
        }
        first = false;
        out += url_encode(key);
        out += '=';
        out += url_encode(value);
    }
    return out;
}

std::string response_text(const X7H6& response) {
    return std::string(response.body.begin(), response.body.end());
}

std::string require_json_string(const std::string& json, const std::string& key, const char* step) {
    const auto value = json_get_string(json, key);
    if (!value) {
        throw std::runtime_error(std::string(step) + ": missing " + key);
    }
    return *value;
}

}  // namespace

X7A1 x7k9_w2q8(const std::string& server, const std::string& username, const std::string& password) {
    X7C7 http;
    const std::string device_id = random_hex(32);
    std::string server_upper = server;
    for (char& ch : server_upper) {
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    }
    const std::string nebula_username = server_upper + "|" + username;

    auto password_headers = nebula_browser_headers();
    password_headers["Content-Type"] = "application/x-www-form-urlencoded";

    const std::string password_body = form_body({
        {"client_id", kClientId},
        {"client_secret", kClientSecret},
        {"grant_type", "password"},
        {"scope", "openid nebula offline_access"},
        {"username", nebula_username},
        {"password", password},
        {"acr_values", "gameId:" + std::string(kMsp1GameId) + " deviceId:" + device_id},
    });

    const X7H6 password_response = http.post(
        kTokenHost,
        kTokenPath,
        std::vector<uint8_t>(password_body.begin(), password_body.end()),
        password_headers,
        true);
    if (password_response.status != 200) {
        throw std::runtime_error("password grant failed: " + response_text(password_response));
    }

    const std::string password_json = response_text(password_response);
    const std::string initial_access_token = require_json_string(password_json, "access_token", "password grant");
    const std::string refresh_token = require_json_string(password_json, "refresh_token", "password grant");

    const std::string jwt_json = decode_jwt_payload_json(initial_access_token);
    std::string login_id = json_get_string(jwt_json, "loginId").value_or("");
    if (login_id.empty()) {
        login_id = json_get_string(jwt_json, "sub").value_or("");
    }
    if (login_id.empty()) {
        throw std::runtime_error("loginId missing in jwt");
    }

    auto profile_headers = nebula_browser_headers();
    profile_headers["Authorization"] = "Bearer " + initial_access_token;
    const X7H6 profile_response = http.get(
        kProfileHost,
        "/profileidentity/v1/logins/" + login_id + "/profiles",
        profile_headers,
        true);
    if (profile_response.status != 200) {
        throw std::runtime_error("profile lookup failed: " + response_text(profile_response));
    }

    const std::string profile_json = response_text(profile_response);
    const std::string profile_id = json_first_array_object_field(profile_json, "id").value_or("");
    if (profile_id.empty()) {
        throw std::runtime_error("profile id missing");
    }

    auto refresh_headers = nebula_browser_headers();
    refresh_headers["Content-Type"] = "application/x-www-form-urlencoded";
    refresh_headers["Authorization"] = std::string("Basic ") + kBasicAuth;

    const std::string refresh_body = form_body({
        {"grant_type", "refresh_token"},
        {"refresh_token", refresh_token},
        {"acr_values",
         "gameId:" + std::string(kMsp1GameId) + " profileId:" + profile_id + " deviceId:" + device_id},
    });

    const X7H6 refresh_response = http.post(
        kTokenHost,
        kTokenPath,
        std::vector<uint8_t>(refresh_body.begin(), refresh_body.end()),
        refresh_headers,
        true);
    if (refresh_response.status != 200) {
        throw std::runtime_error("refresh grant failed: " + response_text(refresh_response));
    }

    const std::string refresh_json = response_text(refresh_response);
    X7A1 result;
    result.access_token = require_json_string(refresh_json, "access_token", "refresh grant");
    result.profile_id = profile_id;
    return result;
}

}  // namespace x7k2::q9m4
