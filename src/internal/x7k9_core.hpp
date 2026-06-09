#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace x7k2::q9m4 {

struct X7V4;
struct X7O5;

struct X7O5 {
    std::map<std::string, X7V4> fields;
};

struct X7V4 {
    enum class Kind { Null, Bool, Int, Double, String, Array, Object } kind = Kind::Null;
    bool b = false;
    int64_t i = 0;
    double d = 0;
    std::string s;
    std::vector<X7V4> items;
    X7O5 object;

    static X7V4 make_null();
    static X7V4 make_bool(bool value);
    static X7V4 make_int(int64_t value);
    static X7V4 make_double(double value);
    static X7V4 make_string(std::string value);
    static X7V4 make_array(std::vector<X7V4> items);
    static X7V4 make_object(X7O5 object);
};

struct X7A1 {
    std::string access_token;
    std::string refresh_token;
    std::string profile_id;
    std::string login_id;
    std::string device_id;
};

struct X7L2 {
    std::string status;
    std::string ticket;
    int64_t actor_id = 0;
};

struct X7R3 {
    int status = 0;
    std::string json_body;
    std::vector<uint8_t> raw_body;
    std::string raw_error;
};

struct X7H6 {
    int status = 0;
    std::vector<uint8_t> body;
    std::string error;
};

struct X7N8 {
    int http_status = 0;
    std::vector<uint8_t> body;
    std::string error;
};

class X7C7 {
public:
    X7H6 post(
        const std::string& host,
        const std::string& path,
        const std::vector<uint8_t>& body,
        const std::map<std::string, std::string>& headers,
        bool use_https = true);

    X7H6 get(
        const std::string& host,
        const std::string& path,
        const std::map<std::string, std::string>& headers,
        bool use_https = true);
};

X7A1 x7k9_w2q8(const std::string& server, const std::string& username, const std::string& password);
X7R3 x7k9_p4r6(const std::string& server, const std::string& method, const std::vector<X7V4>& params);
std::optional<X7L2> x7k9_m3n5(const std::string& json, const std::vector<uint8_t>& raw_amf);
X7V4 x7k9_t7h1(const std::string& ticket);
X7V4 x7k9_t7h1(const std::string& ticket, const std::string& access_token);
X7N8 x7k9_n2c4(const char* host, const char* path, const uint8_t* data, int32_t length);
void x7k9_n2s0();

std::vector<uint8_t> x7k9_e5r9(
    const std::string& method,
    const std::vector<X7V4>& params,
    const std::string& checksum,
    const std::string& session_id = "x");

std::string x7k9_k8s2(const std::vector<X7V4>& arguments);
std::vector<uint8_t> x7k9_z1d3(std::vector<uint8_t> body);

std::string md5_hex(const std::string& data);
std::string md5_hex(const std::vector<uint8_t>& data);
std::string sha1_hex(const std::string& data);
std::string bytes_to_hex(const std::vector<uint8_t>& data);
std::vector<uint8_t> base64url_decode(const std::string& input);
std::string random_hex(size_t byte_count);
std::string to_lower(std::string value);
std::string url_encode(const std::string& value);

std::optional<std::string> json_get_string(const std::string& json, const std::string& key);
std::optional<int64_t> json_get_int(const std::string& json, const std::string& key);
std::optional<std::string> json_first_array_object_field(
    const std::string& json,
    const std::string& field);

}  // namespace x7k2::q9m4
