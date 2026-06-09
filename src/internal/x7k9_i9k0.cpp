#include "internal/x7k9_core.hpp"

#include <cctype>
#include <cstring>

namespace x7k2::q9m4 {
namespace {

size_t skip_ws(const std::string& json, size_t pos) {
    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) {
        ++pos;
    }
    return pos;
}

std::optional<size_t> find_object_key(const std::string& json, const std::string& key, size_t start = 0) {
    const std::string needle = "\"" + key + "\"";
    const size_t pos = json.find(needle, start);
    if (pos == std::string::npos) {
        return std::nullopt;
    }
    return pos;
}

std::optional<int64_t> parse_int_at(const std::string& json, size_t pos) {
    pos = skip_ws(json, pos);
    size_t end = pos;
    while (end < json.size() &&
           (std::isdigit(static_cast<unsigned char>(json[end])) || json[end] == '-')) {
        ++end;
    }
    if (end == pos) {
        return std::nullopt;
    }
    return std::stoll(json.substr(pos, end - pos));
}

std::optional<std::string> parse_string_at(const std::string& json, size_t pos) {
    pos = skip_ws(json, pos);
    if (pos >= json.size() || json[pos] != '"') {
        return std::nullopt;
    }
    ++pos;
    std::string out;
    while (pos < json.size()) {
        const char ch = json[pos++];
        if (ch == '"') {
            return out;
        }
        if (ch == '\\' && pos < json.size()) {
            out.push_back(json[pos++]);
        } else {
            out.push_back(ch);
        }
    }
    return std::nullopt;
}

bool looks_like_session_ticket(const std::string& value) {
    if (value.size() < 8) {
        return false;
    }
    if (!std::isupper(static_cast<unsigned char>(value[0])) ||
        !std::isupper(static_cast<unsigned char>(value[1])) || value[2] != ',') {
        return false;
    }
    int commas = 0;
    for (const char ch : value) {
        if (ch == ',') {
            ++commas;
        }
    }
    return commas >= 2;
}

std::optional<std::string> read_amf3_string_at(const std::vector<uint8_t>& raw, size_t pos) {
    if (pos + 2 >= raw.size() || raw[pos] != 0x06) {
        return std::nullopt;
    }
    uint32_t header = raw[pos + 1];
    size_t cursor = pos + 2;
    if (header & 0x80) {
        if (cursor >= raw.size()) {
            return std::nullopt;
        }
        header = ((header & 0x7F) << 7) | (raw[cursor++] & 0x7F);
        if (header & 0x80) {
            if (cursor >= raw.size()) {
                return std::nullopt;
            }
            header = ((header & 0x7F) << 7) | (raw[cursor++] & 0x7F);
        }
    }
    const size_t length = header >> 1;
    if ((header & 1) == 0 || length == 0 || cursor + length > raw.size()) {
        return std::nullopt;
    }
    return std::string(reinterpret_cast<const char*>(raw.data() + cursor), length);
}

std::optional<std::string> read_amf0_string_at(const std::vector<uint8_t>& raw, size_t pos) {
    if (pos + 3 >= raw.size() || raw[pos] != 0x02) {
        return std::nullopt;
    }
    const uint16_t length = static_cast<uint16_t>((raw[pos + 1] << 8) | raw[pos + 2]);
    if (length == 0 || pos + 3 + length > raw.size()) {
        return std::nullopt;
    }
    return std::string(reinterpret_cast<const char*>(raw.data() + pos + 3), length);
}

std::optional<std::string> read_utf_prefixed_string_at(const std::vector<uint8_t>& raw, size_t pos) {
    if (pos + 2 >= raw.size()) {
        return std::nullopt;
    }
    const uint16_t length = static_cast<uint16_t>((raw[pos] << 8) | raw[pos + 1]);
    if (length == 0 || pos + 2 + length > raw.size()) {
        return std::nullopt;
    }
    return std::string(reinterpret_cast<const char*>(raw.data() + pos + 2), length);
}

std::optional<std::string> try_ticket_candidates(
    const std::vector<uint8_t>& raw,
    size_t start,
    size_t end) {
    for (size_t j = start; j < end; ++j) {
        if (raw[j] == 0x06) {
            if (const auto value = read_amf3_string_at(raw, j); value && looks_like_session_ticket(*value)) {
                return value;
            }
        }
        if (raw[j] == 0x02) {
            if (const auto value = read_amf0_string_at(raw, j); value && looks_like_session_ticket(*value)) {
                return value;
            }
        }
        if (const auto value = read_utf_prefixed_string_at(raw, j); value && looks_like_session_ticket(*value)) {
            return value;
        }
    }
    return std::nullopt;
}

std::optional<std::string> find_ticket_in_amf(const std::vector<uint8_t>& raw) {
    static const char kKey[] = "ticket";
    for (size_t i = 0; i + 6 < raw.size(); ++i) {
        if (std::memcmp(raw.data() + i, kKey, 6) != 0) {
            continue;
        }
        if (const auto value = try_ticket_candidates(raw, i + 6, std::min(i + 256, raw.size()))) {
            return value;
        }
    }

    for (size_t i = 0; i + 8 < raw.size(); ++i) {
        if (!std::isupper(static_cast<unsigned char>(raw[i])) ||
            !std::isupper(static_cast<unsigned char>(raw[i + 1])) || raw[i + 2] != ',') {
            continue;
        }
        std::string candidate;
        for (size_t j = i; j < raw.size(); ++j) {
            const unsigned char ch = raw[j];
            if (ch < 0x20 || ch > 0x7E) {
                break;
            }
            candidate.push_back(static_cast<char>(ch));
        }
        if (looks_like_session_ticket(candidate)) {
            return candidate;
        }
    }

    for (size_t i = 0; i + 4 < raw.size(); ++i) {
        if (raw[i] != 0x06 && raw[i] != 0x02) {
            continue;
        }
        const std::optional<std::string> value =
            raw[i] == 0x06 ? read_amf3_string_at(raw, i) : read_amf0_string_at(raw, i);
        if (value && looks_like_session_ticket(*value)) {
            return value;
        }
    }

    return std::nullopt;
}

}  // namespace

std::optional<X7L2> x7k9_m3n5(const std::string& json, const std::vector<uint8_t>& raw_amf) {
    X7L2 result;

    const auto login_status_pos = find_object_key(json, "loginStatus");
    const size_t scope = login_status_pos.value_or(0);

    result.status = json_get_string(json, "status").value_or("");
    if (result.status.empty() && login_status_pos) {
        const auto status_key = find_object_key(json, "status", *login_status_pos);
        if (status_key) {
            size_t cursor = *status_key + 8;
            cursor = skip_ws(json, cursor);
            if (cursor < json.size() && json[cursor] == ':') {
                result.status = parse_string_at(json, cursor + 1).value_or("");
            }
        }
    }

    const auto actor_pos = find_object_key(json, "actor", scope);
    if (actor_pos) {
        const auto actor_id_pos = find_object_key(json, "ActorId", *actor_pos);
        if (actor_id_pos) {
            size_t cursor = *actor_id_pos + 9;
            cursor = skip_ws(json, cursor);
            if (cursor < json.size() && json[cursor] == ':') {
                result.actor_id = parse_int_at(json, cursor + 1).value_or(0);
            }
        }
    }
    if (result.actor_id == 0) {
        result.actor_id = json_get_int(json, "ActorId").value_or(0);
    }

    result.ticket = find_ticket_in_amf(raw_amf).value_or("");

    const auto ticket_pos = find_object_key(json, "ticket", scope);
    if (ticket_pos) {
        size_t cursor = *ticket_pos + 8;
        cursor = skip_ws(json, cursor);
        if (cursor < json.size() && json[cursor] == ':') {
            const std::string json_ticket = parse_string_at(json, cursor + 1).value_or("");
            if (looks_like_session_ticket(json_ticket)) {
                result.ticket = json_ticket;
            }
        }
    }

    if (result.status.empty() && result.ticket.empty() && result.actor_id == 0) {
        return std::nullopt;
    }
    return result;
}

}  // namespace x7k2::q9m4
