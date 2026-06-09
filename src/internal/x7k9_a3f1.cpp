#include "internal/x7k9_core.hpp"

#include <bit>
#include <cstring>

namespace x7k2::q9m4 {
namespace {

void write_u16_be(std::vector<uint8_t>& out, uint16_t value) {
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>(value & 0xFF));
}

void write_i32_be(std::vector<uint8_t>& out, int32_t value) {
    const auto u = static_cast<uint32_t>(value);
    out.push_back(static_cast<uint8_t>((u >> 24) & 0xFF));
    out.push_back(static_cast<uint8_t>((u >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((u >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>(u & 0xFF));
}

void write_utf8_amf0(std::vector<uint8_t>& out, const std::string& value) {
    write_u16_be(out, static_cast<uint16_t>(value.size()));
    out.insert(out.end(), value.begin(), value.end());
}

void write_u29(std::vector<uint8_t>& out, uint32_t value) {
    value &= 0x1FFFFFFFu;
    if (value < 0x80u) {
        out.push_back(static_cast<uint8_t>(value));
        return;
    }
    if (value < 0x4000u) {
        out.push_back(static_cast<uint8_t>((value >> 7) | 0x80u));
        out.push_back(static_cast<uint8_t>(value & 0x7Fu));
        return;
    }
    if (value < 0x200000u) {
        out.push_back(static_cast<uint8_t>((value >> 14) | 0x80u));
        out.push_back(static_cast<uint8_t>((value >> 7) | 0x80u));
        out.push_back(static_cast<uint8_t>(value & 0x7Fu));
        return;
    }
    out.push_back(static_cast<uint8_t>((value >> 22) | 0x80u));
    out.push_back(static_cast<uint8_t>((value >> 15) | 0x80u));
    out.push_back(static_cast<uint8_t>((value >> 8) | 0x80u));
    out.push_back(static_cast<uint8_t>(value & 0xFFu));
}

void write_u29_literal(std::vector<uint8_t>& out, uint32_t literal) {
    write_u29(out, (literal << 1u) | 1u);
}

void write_amf3_string_inline(std::vector<uint8_t>& out, const std::string& value) {
    write_u29_literal(out, static_cast<uint32_t>(value.size()));
    out.insert(out.end(), value.begin(), value.end());
}

void write_amf3_string(std::vector<uint8_t>& out, const std::string& value) {
    out.push_back(0x06);
    write_amf3_string_inline(out, value);
}

void write_amf3_int(std::vector<uint8_t>& out, int64_t value) {
    if (value >= -268435456 && value <= 268435455) {
        out.push_back(0x04);
        uint32_t encoded = 0;
        if (value < 0) {
            encoded = static_cast<uint32_t>((1u << 29u) + static_cast<int32_t>(value));
        } else {
            encoded = static_cast<uint32_t>(value);
        }
        write_u29(out, encoded);
        return;
    }
    out.push_back(0x05);
    const double d = static_cast<double>(value);
    const auto bits = static_cast<uint64_t>(std::bit_cast<uint64_t>(d));
    for (int shift = 56; shift >= 0; shift -= 8) {
        out.push_back(static_cast<uint8_t>((bits >> shift) & 0xFF));
    }
}

void write_amf3_double(std::vector<uint8_t>& out, double value) {
    out.push_back(0x05);
    const auto bits = static_cast<uint64_t>(std::bit_cast<uint64_t>(value));
    for (int shift = 56; shift >= 0; shift -= 8) {
        out.push_back(static_cast<uint8_t>((bits >> shift) & 0xFF));
    }
}

void write_amf3_value(std::vector<uint8_t>& out, const X7V4& value);

void write_amf3_dynamic_object(std::vector<uint8_t>& out, const X7O5& object) {
    out.push_back(0x0A);
    write_u29_literal(out, (0u << 3u) | (2u << 1u) | 1u);
    write_u29_literal(out, 0u);
    for (const auto& [key, field] : object.fields) {
        write_amf3_string_inline(out, key);
        write_amf3_value(out, field);
    }
    write_u29_literal(out, 0u);
}

void write_amf3_value(std::vector<uint8_t>& out, const X7V4& value) {
    switch (value.kind) {
        case X7V4::Kind::Null:
            out.push_back(0x01);
            break;
        case X7V4::Kind::Bool:
            out.push_back(0x02);
            if (value.b) {
                out.push_back(0x01);
            }
            break;
        case X7V4::Kind::Int:
            write_amf3_int(out, value.i);
            break;
        case X7V4::Kind::Double:
            write_amf3_double(out, value.d);
            break;
        case X7V4::Kind::String:
            write_amf3_string(out, value.s);
            break;
        case X7V4::Kind::Array:
            out.push_back(0x09);
            write_u29_literal(out, 0u);
            write_u29_literal(out, static_cast<uint32_t>(value.items.size()));
            for (const auto& item : value.items) {
                write_amf3_value(out, item);
            }
            break;
        case X7V4::Kind::Object:
            write_amf3_dynamic_object(out, value.object);
            break;
    }
}

void write_amf0_avmplus(std::vector<uint8_t>& out, const X7V4& value) {
    out.push_back(0x11);
    write_amf3_value(out, value);
}

void write_header(
    std::vector<uint8_t>& out,
    const std::string& name,
    bool must_understand,
    const X7V4& body) {
    write_utf8_amf0(out, name);
    out.push_back(must_understand ? 1 : 0);
    write_i32_be(out, 0);
    write_amf0_avmplus(out, body);
}

}  // namespace

X7V4 X7V4::make_null() {
    return {};
}

X7V4 X7V4::make_bool(bool value) {
    X7V4 v;
    v.kind = Kind::Bool;
    v.b = value;
    return v;
}

X7V4 X7V4::make_int(int64_t value) {
    X7V4 v;
    v.kind = Kind::Int;
    v.i = value;
    return v;
}

X7V4 X7V4::make_double(double value) {
    X7V4 v;
    v.kind = Kind::Double;
    v.d = value;
    return v;
}

X7V4 X7V4::make_string(std::string value) {
    X7V4 v;
    v.kind = Kind::String;
    v.s = std::move(value);
    return v;
}

X7V4 X7V4::make_array(std::vector<X7V4> items) {
    X7V4 v;
    v.kind = Kind::Array;
    v.items = std::move(items);
    return v;
}

X7V4 X7V4::make_object(X7O5 object) {
    X7V4 v;
    v.kind = Kind::Object;
    v.object = std::move(object);
    return v;
}

std::vector<uint8_t> x7k9_e5r9(
    const std::string& method,
    const std::vector<X7V4>& params,
    const std::string& checksum,
    const std::string& session_id) {
    std::vector<uint8_t> out;
    write_u16_be(out, 3);
    write_u16_be(out, 3);
    write_header(out, "sessionID", false, X7V4::make_string(session_id));
    write_header(out, "id", false, X7V4::make_string(checksum));
    write_header(out, "needClassName", false, X7V4::make_bool(false));
    write_u16_be(out, 1);
    write_utf8_amf0(out, method);
    write_utf8_amf0(out, "/1");
    write_i32_be(out, 0);
    out.push_back(0x0A);
    write_i32_be(out, static_cast<int32_t>(params.size()));
    for (const auto& param : params) {
        write_amf0_avmplus(out, param);
    }
    return out;
}

}  // namespace x7k2::q9m4
