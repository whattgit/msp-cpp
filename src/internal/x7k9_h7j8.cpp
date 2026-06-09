#include "internal/x7k9_core.hpp"

#include "internal/x7k9_m9p1.hpp"

namespace x7k2::q9m4 {
namespace {

std::string normalize_server(std::string server) {
    server = to_lower(std::move(server));
    if (server == "uk") {
        return "gb";
    }
    return server;
}

}  // namespace

X7R3 x7k9_p4r6(const std::string& server, const std::string& method, const std::vector<X7V4>& params) {
    const std::string host_server = normalize_server(server);
    const std::string host = "ws-" + host_server + ".moviestarplanet.app";
    const std::string checksum = x7k9_k8s2(params);
    const std::vector<uint8_t> payload = x7k9_e5r9(method, params, checksum);
    const std::string path = "/Gateway.aspx?method=" + method;

    const X7N8 response = x7k9_n2c4(
        host.c_str(),
        path.c_str(),
        payload.data(),
        static_cast<int32_t>(payload.size()));

    X7R3 result;
    result.status = response.http_status;
    if (!response.error.empty()) {
        result.raw_error = response.error;
        return result;
    }
    if (response.http_status != 200) {
        result.raw_error = std::string(response.body.begin(), response.body.end());
        return result;
    }
    if (response.body.empty()) {
        result.raw_error = "empty amf response";
        return result;
    }

    result.raw_body = response.body;
    std::vector<uint8_t> decoded_body = x7k9_z1d3(response.body);
    const X7D2 decoded = x7k9_m9p1(decoded_body);
    result.json_body = decoded.json;
    if (result.json_body.empty() || result.json_body.front() != '{') {
        result.raw_error = "amf decode failed";
        result.status = 0;
    }
    return result;
}

}  // namespace x7k2::q9m4
