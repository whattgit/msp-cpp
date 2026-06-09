#include "internal/x7k9_core.hpp"

#include <cctype>

namespace x7k2::q9m4 {
namespace {

size_t skip_ws(const std::string& json, size_t pos) {
    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) {
        ++pos;
    }
    return pos;
}

std::optional<std::string> parse_json_string_at(const std::string& json, size_t& pos) {
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

std::optional<size_t> find_key(const std::string& json, const std::string& key, size_t start = 0) {
    const std::string needle = "\"" + key + "\"";
    const size_t pos = json.find(needle, start);
    if (pos == std::string::npos) {
        return std::nullopt;
    }
    size_t cursor = pos + needle.size();
    cursor = skip_ws(json, cursor);
    if (cursor >= json.size() || json[cursor] != ':') {
        return std::nullopt;
    }
    return cursor + 1;
}

}  // namespace

std::optional<std::string> json_get_string(const std::string& json, const std::string& key) {
    const auto pos = find_key(json, key);
    if (!pos) {
        return std::nullopt;
    }
    size_t cursor = *pos;
    return parse_json_string_at(json, cursor);
}

std::optional<int64_t> json_get_int(const std::string& json, const std::string& key) {
    const auto pos = find_key(json, key);
    if (!pos) {
        return std::nullopt;
    }
    size_t cursor = skip_ws(json, *pos);
    size_t end = cursor;
    while (end < json.size() && (std::isdigit(static_cast<unsigned char>(json[end])) || json[end] == '-')) {
        ++end;
    }
    if (end == cursor) {
        return std::nullopt;
    }
    return std::stoll(json.substr(cursor, end - cursor));
}

std::optional<std::string> json_first_array_object_field(
    const std::string& json,
    const std::string& field) {
    const auto field_pos = find_key(json, field);
    if (!field_pos) {
        return std::nullopt;
    }
    size_t cursor = *field_pos;
    return parse_json_string_at(json, cursor);
}

}  // namespace x7k2::q9m4
