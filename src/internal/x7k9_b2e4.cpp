#include "internal/x7k9_core.hpp"

namespace x7k2::q9m4 {
namespace {

constexpr const char* kNoTicketValue = "XSV7%!5!AX2L8@vn";
constexpr const char* kSalt = "2zKzokBI4^26#oiP";

std::string from_object_inner(const X7V4& value);
std::string from_object_fields(const X7O5& object);

std::string from_array(const std::vector<X7V4>& values) {
    std::string out;
    for (const auto& value : values) {
        out += from_object_inner(value);
    }
    return out;
}

std::string from_object_fields(const X7O5& object) {
    if (object.fields.count("Ticket")) {
        return "";
    }
    std::string out;
    for (const auto& [key, value] : object.fields) {
        (void)key;
        out += from_object_inner(value);
    }
    return out;
}

std::string from_object_inner(const X7V4& value) {
    switch (value.kind) {
        case X7V4::Kind::Null:
            return "";
        case X7V4::Kind::Bool:
            return value.b ? "True" : "False";
        case X7V4::Kind::Int:
            return std::to_string(value.i);
        case X7V4::Kind::Double:
            return std::to_string(static_cast<int64_t>(value.d));
        case X7V4::Kind::String:
            return value.s;
        case X7V4::Kind::Array:
            return from_array(value.items);
        case X7V4::Kind::Object:
            return from_object_fields(value.object);
    }
    return "";
}

std::string get_ticket_value(const std::vector<X7V4>& values) {
    for (const auto& value : values) {
        if (value.kind != X7V4::Kind::Object) {
            continue;
        }
        const auto ticket_it = value.object.fields.find("Ticket");
        if (ticket_it == value.object.fields.end()) {
            continue;
        }
        if (ticket_it->second.kind != X7V4::Kind::String) {
            continue;
        }
        const std::string& ticket = ticket_it->second.s;
        const auto comma = ticket.find(',');
        if (comma == std::string::npos) {
            continue;
        }
        std::vector<std::string> parts;
        size_t start = 0;
        while (start < ticket.size()) {
            const auto next = ticket.find(',', start);
            if (next == std::string::npos) {
                parts.push_back(ticket.substr(start));
                break;
            }
            parts.push_back(ticket.substr(start, next - start));
            start = next + 1;
        }
        if (parts.size() >= 6 && parts[5].size() >= 5) {
            return parts[0] + parts[5].substr(parts[5].size() - 5);
        }
    }
    return kNoTicketValue;
}

}  // namespace

std::string x7k9_k8s2(const std::vector<X7V4>& arguments) {
    return sha1_hex(from_array(arguments) + kSalt + get_ticket_value(arguments));
}

}  // namespace x7k2::q9m4
