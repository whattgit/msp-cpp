#include "internal/x7k9_m9p1.hpp"
#include "internal/x7k9_sxc.hpp"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

extern "C" {
#include "miniz.h"
}

namespace x7k2::q9m4 {
namespace {

enum class Z4K { Null, Undefined, Bool, Number, String, Object, Array, Date, ByteArray, Xml };

struct Z4L {
    int depth = 0;
    size_t nodes = 0;
    bool truncated = false;

    static constexpr int kMaxDepth = 128;
    static constexpr size_t kMaxNodes = 2000000;
    static constexpr size_t kMaxArray = 65536;
    static constexpr size_t kMaxFields = 8192;
    static constexpr int kMaxLoop = 262144;

    bool enter() {
        if (depth >= kMaxDepth) {
            truncated = true;
            return false;
        }
        ++depth;
        return true;
    }

    void leave() {
        if (depth > 0) --depth;
    }

    bool note_node() {
        if (nodes >= kMaxNodes) {
            truncated = true;
            return false;
        }
        ++nodes;
        return true;
    }
};

struct Z4V {
    Z4K kind = Z4K::Null;
    bool b = false;
    double n = 0;
    std::string s;
    std::vector<uint8_t> bytes;
    std::string class_name;
    std::vector<std::pair<std::string, Z4V>> fields;
    std::vector<Z4V> items;
    double date_ms = 0;
};

struct Z4T {
    std::string class_name;
    std::vector<std::string> members;
    bool dynamic = false;
    bool externalizable = false;
};

struct Z4S {
    std::vector<Z4V> obj_refs;
    std::vector<Z4V> array_refs;
};

struct Amf3Context {
    std::vector<std::string> str_refs;
    std::vector<Z4T> traits;
    std::vector<Z4V> obj_refs;
    std::vector<Z4V> array_refs;
};

bool try_decompress_zlib(std::vector<uint8_t>& bytes) {
    if (bytes.size() < 2 || bytes[0] != 0x78) return false;
    mz_ulong dest_len = static_cast<mz_ulong>(bytes.size() * 8 + 256);
    std::vector<uint8_t> dest(dest_len);
    const int rc = mz_uncompress(dest.data(), &dest_len, bytes.data(), static_cast<mz_ulong>(bytes.size()));
    if (rc == MZ_BUF_ERROR) {
        dest_len = static_cast<mz_ulong>(bytes.size() * 32 + 4096);
        dest.assign(dest_len, 0);
        if (mz_uncompress(dest.data(), &dest_len, bytes.data(), static_cast<mz_ulong>(bytes.size())) != MZ_OK) {
            return false;
        }
    } else if (rc != MZ_OK) {
        return false;
    }
    dest.resize(dest_len);
    bytes = std::move(dest);
    return true;
}

class Reader {
public:
    explicit Reader(const uint8_t* data, size_t len, Z4L& limits) : data_(data), len_(len), limits_(limits) {}

    bool eof() const { return pos_ >= end_pos(); }
    size_t pos() const { return pos_; }
    size_t end() const { return end_pos(); }
    uint8_t peek_at(size_t p) const { return p < len_ ? data_[p] : 0; }
    void set_pos(size_t p) { pos_ = p; }
    void advance(size_t n) { pos_ = (std::min)(pos_ + n, end_pos()); }

    void set_section_end(size_t end) { section_end_ = end; }
    void clear_section_end() { section_end_ = 0; }

    const std::vector<Z4V>& object_table() const { return obj_refs_; }
    const std::vector<Z4V>& array_table() const { return array_refs_; }

    size_t end_pos() const { return section_end_ > 0 ? section_end_ : len_; }

    void clear_amf3_tables() {
        str_refs_.clear();
        traits_.clear();
        obj_refs_.clear();
        array_refs_.clear();
    }

    void clear_body_context() {
        clear_amf3_tables();
        amf0_refs_.clear();
    }

    void restore_amf3_context(const Amf3Context& ctx) {
        str_refs_ = ctx.str_refs;
        traits_ = ctx.traits;
        obj_refs_ = ctx.obj_refs;
        array_refs_ = ctx.array_refs;
    }

    Amf3Context capture_amf3_context() const { return {str_refs_, traits_, obj_refs_, array_refs_}; }

    Z4S snapshot() const { return {obj_refs_, array_refs_}; }

    uint8_t peek() const { return pos_ < end_pos() ? data_[pos_] : 0; }

    static bool u29_is_reference(uint32_t u) { return (u & 1u) == 0; }
    static uint32_t u29_reference_index(uint32_t u) { return u >> 1; }
    static uint32_t u29_strip_literal_bit(uint32_t u) { return u >> 1; }

    void read_dynamic_pairs(Z4V& obj) {
        for (int guard = 0; guard < Z4L::kMaxLoop && pos_ < end_pos(); ++guard) {
            const size_t before = pos_;
            const std::string key = read_amf3_string_inline();
            if (key.empty()) break;
            if (obj.fields.size() >= Z4L::kMaxFields) {
                limits_.truncated = true;
                break;
            }
            obj.fields.emplace_back(key, read_dynamic_value(key));
            if (pos_ == before) {
                if (pos_ < end_pos()) ++pos_;
                break;
            }
        }
    }

    Z4V read_ticket_or_amf3_value() {
        if (pos_ >= end_pos()) return make(Z4K::Null);
        if (peek() == 0x11) {
            advance(1);
            return read_amf3_value();
        }
        if (peek() == 0x02 && pos_ + 3 <= end_pos()) {
            const uint16_t slen = static_cast<uint16_t>((data_[pos_ + 1] << 8) | data_[pos_ + 2]);
            if (slen >= 8 && slen < 4096 && pos_ + 3 + slen <= end_pos()) {
                advance(1);
                return make_string(read_data_input_utf());
            }
        }
        const uint8_t marker = peek();
        if (marker >= 0x03 && marker <= 0x0e) return read_amf3_value();
        size_t utf_len = 0;
        if (peek_data_input_utf(utf_len) && utf_len > 0 && utf_len < 4096) {
            return make_string(read_data_input_utf());
        }
        if (marker == 0x00 || marker == 0x01) return read_amf3_value();
        return read_amf3_value();
    }

    Z4V read_dynamic_value(const std::string& key) {
        if (key == "Ticket" || key == "ticket" || key == "anyAttribute") return read_ticket_or_amf3_value();
        return read_amf3_value();
    }

    uint16_t read_u16() {
        if (pos_ + 2 > end_pos()) return 0;
        const uint16_t v = static_cast<uint16_t>((data_[pos_] << 8) | data_[pos_ + 1]);
        pos_ += 2;
        return v;
    }

    uint8_t read_u8() {
        if (pos_ >= end_pos()) return 0;
        return data_[pos_++];
    }

    int32_t read_i32_be() {
        if (pos_ + 4 > end_pos()) return 0;
        const int32_t v = (static_cast<int32_t>(data_[pos_]) << 24) |
                          (static_cast<int32_t>(data_[pos_ + 1]) << 16) |
                          (static_cast<int32_t>(data_[pos_ + 2]) << 8) | data_[pos_ + 3];
        pos_ += 4;
        return v;
    }

    double read_f64_be() {
        if (pos_ + 8 > end_pos()) return 0;
        const uint64_t bits =
            (static_cast<uint64_t>(data_[pos_]) << 56) | (static_cast<uint64_t>(data_[pos_ + 1]) << 48) |
            (static_cast<uint64_t>(data_[pos_ + 2]) << 40) | (static_cast<uint64_t>(data_[pos_ + 3]) << 32) |
            (static_cast<uint64_t>(data_[pos_ + 4]) << 24) | (static_cast<uint64_t>(data_[pos_ + 5]) << 16) |
            (static_cast<uint64_t>(data_[pos_ + 6]) << 8) | static_cast<uint64_t>(data_[pos_ + 7]);
        pos_ += 8;
        double v = 0;
        std::memcpy(&v, &bits, sizeof(v));
        return v;
    }

    uint32_t read_u29() {
        if (pos_ >= end_pos()) return 0;
        uint32_t result = 0;
        int n = 0;
        uint8_t b = read_u8();
        while ((b & 0x80) != 0 && n < 3 && pos_ < end_pos()) {
            result <<= 7;
            result |= static_cast<uint32_t>(b & 0x7f);
            b = read_u8();
            ++n;
        }
        if (n < 3) {
            result <<= 7;
            result |= static_cast<uint32_t>(b);
        } else {
            result <<= 8;
            result |= static_cast<uint32_t>(b);
            if (result & 0x10000000u) {
                result = (result << 1) + 1;
            }
        }
        return result;
    }

    int32_t read_int29() {
        if (pos_ >= end_pos()) return 0;
        uint32_t result = 0;
        int n = 0;
        uint8_t b = read_u8();
        while ((b & 0x80) != 0 && n < 3 && pos_ < end_pos()) {
            result <<= 7;
            result |= static_cast<uint32_t>(b & 0x7f);
            b = read_u8();
            ++n;
        }
        if (n < 3) {
            result <<= 7;
            result |= static_cast<uint32_t>(b);
        } else {
            result <<= 8;
            result |= static_cast<uint32_t>(b);
            if (result & 0x10000000u) {
                result -= 0x20000000u;
            }
        }
        return static_cast<int32_t>(result);
    }

    std::string read_amf0_string() {
        const uint16_t slen = read_u16();
        if (slen == 0) return {};
        if (pos_ + slen > end_pos()) return {};
        std::string s(reinterpret_cast<const char*>(data_ + pos_), slen);
        pos_ += slen;
        return s;
    }

    std::string read_amf3_string_inline(bool* ends_with_plus = nullptr) {
        if (ends_with_plus) *ends_with_plus = false;
        const uint32_t header = read_u29();
        if (u29_is_reference(header)) {
            const size_t idx = u29_reference_index(header);
            if (idx < str_refs_.size()) return str_refs_[idx];
            return {};
        }
        const size_t slen = u29_reference_index(header);
        if (slen == 0) return {};
        if (pos_ + slen > end_pos()) return {};
        std::string s(reinterpret_cast<const char*>(data_ + pos_), slen);
        pos_ += slen;
        if (ends_with_plus && !s.empty() && s.back() == '+') *ends_with_plus = true;
        str_refs_.push_back(s);
        return s;
    }

    static bool is_valid_member_name(const std::string& s) {
        if (s.size() < 2 || s.size() > 64) return false;
        if (!std::isalpha(static_cast<unsigned char>(s[0]))) return false;
        for (unsigned char c : s) {
            if (!std::isalnum(c) && c != '_') return false;
        }
        return true;
    }

    static bool is_blazeds_u29_byte(uint8_t b) {
        return (b & 1u) != 0 && b <= 0x47 && (b >> 1) >= 2;
    }

    size_t find_trait_value_anchor(size_t from, size_t max_scan = 2048) const {
        const size_t bound = (std::min)(end_pos(), from + max_scan);
        for (size_t i = from; i + 4 <= bound; ++i) {
            if (data_[i] == 0x08 && data_[i + 1] == 0x04 && data_[i + 2] == 0x02 && data_[i + 3] == 0x05) {
                return i;
            }
        }
        for (size_t i = from; i + 10 <= bound; ++i) {
            if (data_[i] == 0x08 && data_[i + 1] == 0x01 && data_[i + 2] == 0x42) return i;
        }
        return end_pos();
    }

    std::string read_blazeds_alpha_name(size_t& pos, size_t end) {
        if (pos >= end) return {};
        const uint8_t b = data_[pos];
        if (!std::isalpha(static_cast<unsigned char>(b))) return {};
        size_t e = pos + 1;
        while (e < end) {
            const uint8_t c = data_[e];
            if (c == '+' || c == '/') break;
            if (c >= '0' && c <= '9' && e + 1 < end && data_[e + 1] >= 'A' && data_[e + 1] <= 'Z') break;
            if (is_blazeds_u29_byte(c)) break;
            if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') break;
            ++e;
        }
        std::string s(reinterpret_cast<const char*>(data_ + pos), e - pos);
        pos = e;
        return s;
    }

    static uint32_t read_u29_at_pos(const uint8_t* data, size_t end, size_t& at) {
        if (at >= end) return 0;
        uint32_t result = 0;
        int n = 0;
        uint8_t b = data[at++];
        while ((b & 0x80) && n < 3 && at < end) {
            result = (result << 7) | (b & 0x7F);
            b = data[at++];
            ++n;
        }
        if (n < 3) {
            result = (result << 7) | b;
        } else {
            result = (result << 8) | b;
            if (result & 0x10000000u) result = (result << 1) + 1;
        }
        return result;
    }

    void read_trait_tail_members(std::vector<std::string>& members, size_t member_count, size_t max_blob = 256) {
        const size_t blob_end = (std::min)(pos_ + max_blob, end_pos());
        while (pos_ < blob_end && members.size() < member_count) {
            const size_t before = pos_;
            size_t trial = before;
            const uint32_t wire = read_u29_at_pos(data_, end_pos(), trial);
            if (u29_is_reference(wire) && trial == before + 1) {
                pos_ = trial;
                members.push_back("field_" + std::to_string(members.size()));
                continue;
            }
            std::string m = read_amf3_string_inline();
            if (is_valid_member_name(m)) {
                members.push_back(std::move(m));
                continue;
            }
            pos_ = before;
            const std::string alpha = read_blazeds_alpha_name(pos_, blob_end);
            if (is_valid_member_name(alpha)) {
                members.push_back(alpha);
                continue;
            }
            pos_ = before + 1;
        }
    }

    void read_blazeds_packed_members(size_t start, size_t end, std::vector<std::string>& members, size_t member_count) {
        size_t pos = start;
        while (pos < end && members.size() < member_count) {
            const uint8_t b = data_[pos];
            if (is_blazeds_u29_byte(b)) break;
            if (b == '+' || b == '/') {
                ++pos;
                continue;
            }
            if (b >= '0' && b <= '9') {
                ++pos;
                continue;
            }
            const std::string name = read_blazeds_alpha_name(pos, end);
            if (is_valid_member_name(name)) {
                members.push_back(name);
            } else if (pos < end) {
                ++pos;
            }
        }
        while (pos < end && members.size() < member_count) {
            const uint8_t b = data_[pos];
            if (!is_blazeds_u29_byte(b)) {
                ++pos;
                continue;
            }
            const size_t ln = b >> 1;
            ++pos;
            if (pos + ln > end) break;
            std::string s(reinterpret_cast<const char*>(data_ + pos), ln);
            pos += ln;
            if (is_valid_member_name(s)) members.push_back(s);
        }
    }

    void read_trait_member_names(size_t member_count, std::vector<std::string>& members) {
        members.clear();
        members.reserve(member_count);

        if (member_count <= 8) {
            for (size_t i = 0; i < member_count; ++i) {
                std::string m = read_amf3_string_inline();
                if (!m.empty() && m.back() == '+') m.pop_back();
                members.push_back(std::move(m));
            }
            return;
        }

        bool packed = false;
        while (members.size() < member_count && !packed) {
            const size_t before = pos_;
            bool had_plus = false;
            std::string m = read_amf3_string_inline(&had_plus);
            if (!m.empty() && m.back() == '+') m.pop_back();
            if (!is_valid_member_name(m)) {
                pos_ = before;
                packed = true;
                break;
            }
            members.push_back(std::move(m));
            if (had_plus) packed = true;
        }

        if (packed || members.size() < member_count) {
            if (member_count > 32) {
                const size_t blob_start = pos_;
                const size_t anchor = find_trait_value_anchor(blob_start);
                const size_t blob_end =
                    (anchor > blob_start && anchor < end_pos()) ? anchor : (std::min)(blob_start + 2048, end_pos());
                read_blazeds_packed_members(pos_, blob_end, members, member_count);
                pos_ = blob_end;
            } else {
                read_trait_tail_members(members, member_count);
            }
        }

        while (members.size() < member_count) {
            members.push_back("field_" + std::to_string(members.size()));
        }
        if (members.size() > member_count) members.resize(member_count);
    }

    std::string read_data_input_utf() {
        const uint16_t slen = read_u16();
        if (slen == 0 || pos_ + slen > end_pos()) return {};
        std::string s(reinterpret_cast<const char*>(data_ + pos_), slen);
        pos_ += slen;
        return s;
    }

    bool peek_data_input_utf(size_t& out_len) const {
        if (pos_ + 2 > end_pos()) return false;
        out_len = static_cast<size_t>((data_[pos_] << 8) | data_[pos_ + 1]);
        return out_len > 0 && pos_ + 2 + out_len <= end_pos();
    }

    void read_externalizable_object(Z4V& obj) {
        const std::string& class_name = obj.class_name;
        const auto add = [&](const std::string& key, Z4V val) {
            if (obj.fields.size() >= Z4L::kMaxFields) {
                limits_.truncated = true;
                return false;
            }
            obj.fields.emplace_back(key, std::move(val));
            return true;
        };

        if (class_name.find("TicketHeader") != std::string::npos) {
            add("Ticket", read_ticket_or_amf3_value());
            if (pos_ < end_pos()) add("anyAttribute", read_ticket_or_amf3_value());
            return;
        }

        size_t utf_len = 0;
        if (peek_data_input_utf(utf_len) && utf_len > 0 && utf_len < 4096 && peek() < 0x02) {
            add("value", make_string(read_data_input_utf()));
            return;
        }

        const size_t bound = end_pos();
        for (int guard = 0; guard < Z4L::kMaxLoop && pos_ < bound && obj.fields.size() < 64; ++guard) {
            const size_t before = pos_;
            if (peek() == 0x11) {
                advance(1);
                if (!add("[" + std::to_string(obj.fields.size()) + "]", read_amf3_value())) break;
                continue;
            }
            if (peek() <= 1) {
                if (!add("[" + std::to_string(obj.fields.size()) + "]", make_bool(read_u8() != 0))) break;
                continue;
            }
            if (peek_data_input_utf(utf_len) && utf_len < 8192) {
                if (!add("[" + std::to_string(obj.fields.size()) + "]", make_string(read_data_input_utf()))) break;
                continue;
            }
            if (pos_ == before) {
                if (pos_ < bound) ++pos_;
                break;
            }
        }
    }

    Z4V read_amf3_value() {
        if (pos_ >= end_pos()) return {};
        if (amf3_depth_ >= Z4L::kMaxDepth) {
            limits_.truncated = true;
            Z4V v = make(Z4K::Undefined);
            v.s = "(max AMF3 depth)";
            return v;
        }
        struct DepthGuard {
            int& d;
            explicit DepthGuard(int& depth) : d(depth) { ++d; }
            ~DepthGuard() { --d; }
        } guard(amf3_depth_);
        const uint8_t type = read_u8();
        switch (type) {
            case 0x00: return make(Z4K::Undefined);
            case 0x01: return make(Z4K::Null);
            case 0x02: return make_bool(false);
            case 0x03: return make_bool(true);
            case 0x04: return make_number(static_cast<double>(read_int29()));
            case 0x05: return make_number(read_f64_be());
            case 0x06: return make_string(read_amf3_string_inline());
            case 0x07: return read_amf3_xml();
            case 0x08: return read_amf3_date();
            case 0x09: return read_amf3_array();
            case 0x0a: return read_amf3_object();
            case 0x0b: return read_amf3_xml();
            case 0x0c: return read_amf3_byte_array();
            case 0x0d: return read_amf3_vector();
            case 0x0e: return read_amf3_dictionary();
            default: {
                Z4V v = make(Z4K::Undefined);
                char buf[16];
                sprintf_s(buf, "0x%02x", type);
                v.s = std::string("unknown AMF3 type ") + buf;
                return v;
            }
        }
    }

    Z4V read_amf0_with_marker(uint8_t marker) {
        switch (marker) {
            case 0x00: return make_number(read_f64_be());
            case 0x01: return make_bool(read_u8() != 0);
            case 0x02: return make_string(read_amf0_string());
            case 0x03: return read_amf0_object(false);
            case 0x05: return make(Z4K::Null);
            case 0x06: return make(Z4K::Undefined);
            case 0x08: return read_amf0_ecma_array();
            case 0x0a: return read_amf0_strict_array();
            case 0x07: return read_amf0_reference();
            case 0x0b:
                read_f64_be();
                read_u16();
                return make(Z4K::Null);
            case 0x11:
                return read_amf3_value();
            case 0x17:
                return read_amf3_value();
            case 0x10:
                return read_amf0_object(true);
            case 0x09:
                return make(Z4K::Null);
            case 0x0c:
                return read_amf0_xml_document();
            default: {
                Z4V v = make(Z4K::Undefined);
                char buf[16];
                sprintf_s(buf, "0x%02x", marker);
                v.s = std::string("unknown AMF0 type ") + buf;
                return v;
            }
        }
    }

    Z4V read_remoting_payload() {
        if (pos_ >= end_pos()) return make(Z4K::Null);
        if (peek() == 0x11) {
            advance(1);
            return read_amf3_value();
        }
        if (peek() == 0x05) {
            advance(1);
            return make(Z4K::Null);
        }
        return read_amf0_with_marker(read_u8());
    }

    Z4V read_request_args() {
        if (pos_ >= end_pos()) return make(Z4K::Null);
        if (peek() == 0x11) {
            advance(1);
            return read_amf3_value();
        }
        if (peek() != 0x0a) return read_remoting_payload();

        advance(1);
        uint32_t count = read_u32_be();
        if (count > Z4L::kMaxArray) {
            limits_.truncated = true;
            count = static_cast<uint32_t>(Z4L::kMaxArray);
        }
        Z4V arr;
        arr.kind = Z4K::Array;
        arr.items.reserve(count);
        for (uint32_t i = 0; i < count && pos_ < end_pos(); ++i) {
            arr.items.push_back(read_remoting_payload());
        }
        return arr;
    }

    Z4V read_value() {
        if (pos_ >= end_pos()) return {};
        return read_amf0_with_marker(read_u8());
    }

private:
    static Z4V make(Z4K k) {
        Z4V v;
        v.kind = k;
        return v;
    }
    static Z4V make_bool(bool b) {
        Z4V v;
        v.kind = Z4K::Bool;
        v.b = b;
        return v;
    }
    static Z4V make_number(double n) {
        Z4V v;
        v.kind = Z4K::Number;
        v.n = n;
        return v;
    }
    static Z4V make_string(std::string s) {
        Z4V v;
        v.kind = Z4K::String;
        v.s = std::move(s);
        return v;
    }

    Z4V read_amf0_reference() {
        const uint16_t idx = read_u16();
        if (idx < amf0_refs_.size()) return amf0_refs_[idx];
        Z4V v = make(Z4K::Object);
        v.s = "AMF0 ref #" + std::to_string(idx);
        return v;
    }

    void register_amf0_ref(const Z4V& v) {
        if (amf0_refs_.size() < 65535) amf0_refs_.push_back(v);
    }

    Z4V read_amf0_object(bool typed) {
        Z4V v;
        v.kind = Z4K::Object;
        if (typed) v.class_name = read_amf0_string();
        register_amf0_ref(v);
        read_amf0_fields(v);
        return v;
    }

    void read_amf0_fields(Z4V& obj) {
        for (int guard = 0; guard < Z4L::kMaxLoop && pos_ < end_pos(); ++guard) {
            const size_t before = pos_;
            const std::string key = read_amf0_string();
            if (key.empty()) {
                if (pos_ < end_pos()) read_u8();
                break;
            }
            if (pos_ >= end_pos()) break;
            if (obj.fields.size() >= Z4L::kMaxFields) {
                limits_.truncated = true;
                break;
            }
            const uint8_t marker = read_u8();
            if (marker == 0x09) break;
            obj.fields.emplace_back(key, read_amf0_with_marker(marker));
            if (pos_ == before) {
                if (pos_ < end_pos()) ++pos_;
                break;
            }
        }
    }

    Z4V read_amf0_ecma_array() {
        read_u32_be();
        Z4V v;
        v.kind = Z4K::Object;
        register_amf0_ref(v);
        read_amf0_fields(v);
        return v;
    }

    Z4V read_amf0_strict_array() {
        uint32_t count = read_u32_be();
        if (count > Z4L::kMaxArray) {
            limits_.truncated = true;
            count = static_cast<uint32_t>(Z4L::kMaxArray);
        }
        Z4V v;
        v.kind = Z4K::Array;
        register_amf0_ref(v);
        v.items.reserve(count);
        for (uint32_t i = 0; i < count && pos_ < end_pos(); ++i) {
            v.items.push_back(read_remoting_payload());
        }
        return v;
    }

    uint32_t read_u32_be() {
        if (pos_ + 4 > end_pos()) return 0;
        const uint32_t v = (static_cast<uint32_t>(data_[pos_]) << 24) |
                           (static_cast<uint32_t>(data_[pos_ + 1]) << 16) |
                           (static_cast<uint32_t>(data_[pos_ + 2]) << 8) | data_[pos_ + 3];
        pos_ += 4;
        return v;
    }

    Z4V read_amf3_xml() {
        const uint32_t header = read_u29();
        if (u29_is_reference(header)) {
            const size_t idx = u29_reference_index(header);
            if (idx < obj_refs_.size()) return obj_refs_[idx];
            Z4V v = make(Z4K::Xml);
            v.s = "xml ref #" + std::to_string(idx);
            return v;
        }
        const size_t n = u29_reference_index(header);
        if (pos_ + n > end_pos()) return make(Z4K::Xml);
        Z4V v;
        v.kind = Z4K::Xml;
        v.s.assign(reinterpret_cast<const char*>(data_ + pos_), n);
        pos_ += n;
        obj_refs_.push_back(v);
        return v;
    }

    Z4V read_amf3_date() {
        const uint32_t header = read_u29();
        Z4V v;
        v.kind = Z4K::Date;
        if (u29_is_reference(header)) {
            const size_t idx = u29_reference_index(header);
            if (idx < obj_refs_.size()) return obj_refs_[idx];
            v.s = "date ref #" + std::to_string(idx);
            return v;
        }
        v.date_ms = read_f64_be();
        v.n = v.date_ms;
        obj_refs_.push_back(v);
        return v;
    }

    Z4V read_amf3_byte_array() {
        const uint32_t header = read_u29();
        Z4V v;
        v.kind = Z4K::ByteArray;
        if (u29_is_reference(header)) {
            const size_t idx = u29_reference_index(header);
            if (idx < obj_refs_.size()) return obj_refs_[idx];
            v.s = "bytearray ref #" + std::to_string(idx);
            return v;
        }
        const size_t n = u29_reference_index(header);
        if (pos_ + n > end_pos()) return v;
        v.bytes.assign(data_ + pos_, data_ + pos_ + n);
        pos_ += n;
        try_decompress_zlib(v.bytes);
        obj_refs_.push_back(v);
        return v;
    }

    Z4V read_amf3_vector() {
        const uint32_t header = read_u29();
        Z4V v;
        v.kind = Z4K::Array;
        if (u29_is_reference(header)) {
            const size_t idx = u29_reference_index(header);
            if (idx < array_refs_.size()) return array_refs_[idx];
            v.s = "vector ref #" + std::to_string(idx);
            return v;
        }
        size_t count = u29_reference_index(header);
        if (count > Z4L::kMaxArray) {
            limits_.truncated = true;
            count = Z4L::kMaxArray;
        }
        const std::string type_name = read_amf3_string_inline();
        v.class_name = type_name.empty() ? "Vector" : ("Vector<" + type_name + ">");
        v.items.reserve(count);
        for (size_t i = 0; i < count && pos_ < end_pos(); ++i) {
            v.items.push_back(read_amf3_value());
        }
        array_refs_.push_back(std::move(v));
        return array_refs_.back();
    }

    Z4V read_amf3_dictionary() {
        const uint32_t header = read_u29();
        if (u29_is_reference(header)) {
            const size_t idx = u29_reference_index(header);
            if (idx < obj_refs_.size()) return obj_refs_[idx];
            Z4V v = make(Z4K::Object);
            v.class_name = "Dictionary (ref)";
            v.s = "dictionary ref #" + std::to_string(idx);
            return v;
        }
        Z4V obj;
        obj.kind = Z4K::Object;
        obj.class_name = "Dictionary";
        const size_t count = u29_reference_index(header);
        for (size_t i = 0; i < count && pos_ < end_pos(); ++i) {
            const std::string key = read_amf3_string_inline();
            if (key.empty()) break;
            obj.fields.emplace_back(key, read_amf3_value());
        }
        obj_refs_.push_back(std::move(obj));
        return obj_refs_.back();
    }

    bool read_trait_description(uint32_t header, std::string& class_name, std::vector<std::string>& members,
                                bool& dynamic, bool& externalizable, bool& trait_resolved) {
        externalizable = false;
        dynamic = false;
        trait_resolved = true;
        if (u29_is_reference(header)) {
            const size_t idx = u29_reference_index(header);
            if (idx < traits_.size()) {
                class_name = traits_[idx].class_name;
                members = traits_[idx].members;
                dynamic = traits_[idx].dynamic;
                externalizable = traits_[idx].externalizable;
            } else {
                trait_resolved = false;
                class_name = "trait ref #" + std::to_string(idx);
            }
            return true;
        }

        header = u29_strip_literal_bit(header);
        const uint32_t encoding = header & 3u;
        const size_t member_count = header >> 2;
        externalizable = encoding == 1 || encoding == 3;
        dynamic = encoding == 2 || encoding == 3;

        class_name.clear();
        members.clear();
        class_name = read_amf3_string_inline();
        if (member_count > Z4L::kMaxFields) {
            limits_.truncated = true;
        }
        const size_t name_count = (std::min)(member_count, Z4L::kMaxFields);
        read_trait_member_names(name_count, members);
        traits_.push_back({class_name, members, dynamic, externalizable});
        return true;
    }

    static bool msp_values_are_amf3(const std::string& class_name) {
        if (class_name.find("MovieStarPlanet") != std::string::npos) return true;
        if (class_name.find("flex.messaging") != std::string::npos) return true;
        if (class_name.find("TicketHeader") != std::string::npos) return false;
        return false;
    }

    bool peek_amf3_typed_value() const {
        if (pos_ >= end_pos()) return false;
        const uint8_t b = peek();
        return b == 0x11 || (b >= 0x00 && b <= 0x0e);
    }

    Z4V read_amf0_xml_document() {
        if (pos_ + 4 > end_pos()) return make(Z4K::Xml);
        pos_ += 4;
        return make(Z4K::Xml);
    }

    Z4V read_amf3_object() {
        const uint32_t wire = read_u29();
        if (u29_is_reference(wire)) {
            const size_t idx = u29_reference_index(wire);
            Z4V v = make(Z4K::Object);
            v.s = "object ref #" + std::to_string(idx);
            return v;
        }

        const uint32_t header = u29_strip_literal_bit(wire);

        Z4V obj;
        obj.kind = Z4K::Object;
        std::vector<std::string> members;
        bool dynamic = false;
        bool externalizable = false;
        bool trait_resolved = true;
        read_trait_description(header, obj.class_name, members, dynamic, externalizable, trait_resolved);

        if (!trait_resolved) {
            obj.s = "(unresolved trait reference)";
            obj_refs_.push_back(obj);
            return obj_refs_.back();
        }

        const bool msp_sealed_dto =
            externalizable && msp_values_are_amf3(obj.class_name) && peek_amf3_typed_value();
        if (externalizable && !msp_sealed_dto) {
            read_externalizable_object(obj);
            if (dynamic) read_dynamic_pairs(obj);
            if (obj.fields.empty()) obj.s = "(externalizable, no values decoded)";
        } else {
            obj.fields.reserve(members.size());
            for (size_t i = 0; i < members.size(); ++i) {
                obj.fields.emplace_back(members[i], read_amf3_value());
            }
            if (dynamic) read_dynamic_pairs(obj);
        }

        obj_refs_.push_back(std::move(obj));
        return obj_refs_.back();
    }

    Z4V read_amf3_array() {
        const uint32_t header = read_u29();
        Z4V v;
        v.kind = Z4K::Array;
        if (u29_is_reference(header)) {
            const size_t idx = u29_reference_index(header);
            if (idx < array_refs_.size()) return array_refs_[idx];
            v.s = "array ref #" + std::to_string(idx);
            return v;
        }

        size_t dense = u29_reference_index(header);
        if (dense > Z4L::kMaxArray) {
            limits_.truncated = true;
            dense = Z4L::kMaxArray;
        }

        const std::string ecma_name = read_amf3_string_inline();

        if (ecma_name.empty()) {
            v.items.reserve(dense);
            for (size_t i = 0; i < dense; ++i) v.items.push_back(read_amf3_value());
        } else {
            v.class_name = ecma_name;
            for (int guard = 0; guard < Z4L::kMaxLoop && pos_ < end_pos(); ++guard) {
                const size_t before = pos_;
                const std::string key = read_amf3_string_inline();
                if (key.empty()) break;
                v.fields.emplace_back(key, read_amf3_value());
                if (pos_ == before) {
                    if (pos_ < end_pos()) ++pos_;
                    break;
                }
            }
            v.items.reserve(dense);
            for (size_t i = 0; i < dense; ++i) v.items.push_back(read_amf3_value());
        }

        array_refs_.push_back(std::move(v));
        return array_refs_.back();
    }

    const uint8_t* data_;
    size_t len_;
    size_t pos_ = 0;
    size_t section_end_ = 0;
    int amf3_depth_ = 0;
    Z4L& limits_;
    std::vector<std::string> str_refs_;
    std::vector<Z4T> traits_;
    std::vector<Z4V> obj_refs_;
    std::vector<Z4V> array_refs_;
    std::vector<Z4V> amf0_refs_;
};

std::string z9e1(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    sprintf_s(buf, "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    return out;
}

void z9jc(const Z4V& v, std::ostringstream& out, int depth) {
    if (depth > Z4L::kMaxDepth) {
        out << "null";
        return;
    }
    switch (v.kind) {
        case Z4K::Null:
        case Z4K::Undefined:
            out << "null";
            break;
        case Z4K::Bool:
            out << (v.b ? "true" : "false");
            break;
        case Z4K::Number:
        case Z4K::Date:
            if (std::floor(v.n) == v.n && std::abs(v.n) < 1e15) {
                out << static_cast<long long>(v.n);
            } else {
                char buf[64];
                sprintf_s(buf, "%.17g", v.n);
                out << buf;
            }
            break;
        case Z4K::String:
        case Z4K::Xml:
            out << '"' << z9e1(v.s) << '"';
            break;
        case Z4K::ByteArray:
            out << '"' << z9e1(std::string(v.bytes.begin(), v.bytes.end())) << '"';
            break;
        case Z4K::Array: {
            out << '[';
            size_t idx = 0;
            for (const auto& item : v.items) {
                z9jc(item, out, depth + 1);
                if (++idx < v.items.size() || !v.fields.empty()) {
                    out << ',';
                }
            }
            for (size_t i = 0; i < v.fields.size(); ++i) {
                out << '"' << z9e1(v.fields[i].first) << "\":";
                z9jc(v.fields[i].second, out, depth + 1);
                if (i + 1 < v.fields.size()) {
                    out << ',';
                }
            }
            out << ']';
            break;
        }
        case Z4K::Object: {
            static const std::string kClassKey =
                detail::x7_sxs({0xacu, 0xbdu, 0x0du, 0x45u, 0x50u, 0xabu, 0x8fu, 0xe8u, 0xe0u});
            out << '{';
            size_t i = 0;
            const size_t total = v.fields.size() + (v.class_name.empty() ? 0 : 1);
            if (!v.class_name.empty()) {
                out << '"' << kClassKey << "\":\"" << z9e1(v.class_name) << '"';
                if (!v.fields.empty()) {
                    out << ',';
                }
                ++i;
            }
            for (const auto& kv : v.fields) {
                out << '"' << z9e1(kv.first) << "\":";
                z9jc(kv.second, out, depth + 1);
                if (++i < total) {
                    out << ',';
                }
            }
            out << '}';
            break;
        }
    }
}

bool remoting_target_is_response(const std::string& target) {
    static constexpr const char* kSuffixes[] = {"/onResult", "/onStatus", "/onDebugEvents"};
    for (const char* suffix : kSuffixes) {
        const size_t n = std::strlen(suffix);
        if (target.size() >= n && target.compare(target.size() - n, n, suffix) == 0) return true;
    }
    return false;
}

Z4V read_length_payload(Reader& r, size_t total_len, int32_t len_field, Z4L& limits,
                          const std::string* target_uri = nullptr) {
    (void)limits;
    const size_t start = r.pos();
    auto decode_one = [&](Reader& rd) -> Z4V {
        const size_t inner_start = rd.pos();
        Z4V v = (target_uri && !remoting_target_is_response(*target_uri)) ? rd.read_request_args()
                                                                            : rd.read_remoting_payload();
        const bool empty_obj =
            (v.kind == Z4K::Object || v.kind == Z4K::Array) && v.fields.empty() && v.items.empty();
        if (empty_obj && rd.pos() + 2 < rd.end()) {
            rd.set_pos(inner_start);
            if (rd.pos() < rd.end() && rd.peek_at(rd.pos()) == 0x11) {
                rd.advance(1);
            }
            const Z4V v3 = rd.read_amf3_value();
            if (!v3.fields.empty() || !v3.items.empty() || v3.kind == Z4K::String || v3.kind == Z4K::Number) {
                v = v3;
            } else {
                rd.set_pos(inner_start);
                v = (target_uri && !remoting_target_is_response(*target_uri)) ? rd.read_request_args()
                                                                            : rd.read_remoting_payload();
            }
        }
        return v;
    };

    Z4V v = decode_one(r);

    if (len_field > 0) {
        const size_t declared_end = (std::min)(start + static_cast<size_t>(len_field), total_len);
        if (r.pos() < declared_end) r.set_pos(declared_end);
    }
    return v;
}

bool parse_ref_index(const std::string& s, const char* prefix, size_t& idx_out) {
    const size_t pfx = std::strlen(prefix);
    if (s.size() <= pfx || s.compare(0, pfx, prefix) != 0) return false;
    try {
        idx_out = static_cast<size_t>(std::stoull(s.substr(pfx)));
        return true;
    } catch (...) {
        return false;
    }
}

void resolve_value_refs(Z4V& v, const std::vector<Z4V>& obj_refs, const std::vector<Z4V>& array_refs, int depth,
                        std::unordered_set<size_t>* active_objs = nullptr,
                        std::unordered_set<size_t>* active_arrs = nullptr) {
    if (depth > Z4L::kMaxDepth) return;

    std::unordered_set<size_t> local_objs;
    std::unordered_set<size_t> local_arrs;
    if (!active_objs) active_objs = &local_objs;
    if (!active_arrs) active_arrs = &local_arrs;

    if (v.kind == Z4K::Object && v.fields.empty() && !v.s.empty()) {
        size_t idx = 0;
        if (parse_ref_index(v.s, "object ref #", idx) && idx < obj_refs.size()) {
            if (active_objs->count(idx)) return;
            active_objs->insert(idx);
            v = obj_refs[idx];
            v.s.clear();
            for (auto& kv : v.fields) resolve_value_refs(kv.second, obj_refs, array_refs, depth + 1, active_objs, active_arrs);
            for (auto& item : v.items) resolve_value_refs(item, obj_refs, array_refs, depth + 1, active_objs, active_arrs);
            active_objs->erase(idx);
            return;
        }
    } else if (v.kind == Z4K::Array && v.items.empty() && v.fields.empty() && !v.s.empty()) {
        size_t idx = 0;
        if (parse_ref_index(v.s, "array ref #", idx) && idx < array_refs.size()) {
            if (active_arrs->count(idx)) return;
            active_arrs->insert(idx);
            v = array_refs[idx];
            v.s.clear();
            for (auto& kv : v.fields) resolve_value_refs(kv.second, obj_refs, array_refs, depth + 1, active_objs, active_arrs);
            for (auto& item : v.items) resolve_value_refs(item, obj_refs, array_refs, depth + 1, active_objs, active_arrs);
            active_arrs->erase(idx);
            return;
        }
    }

    for (auto& kv : v.fields) resolve_value_refs(kv.second, obj_refs, array_refs, depth + 1, active_objs, active_arrs);
    for (auto& item : v.items) resolve_value_refs(item, obj_refs, array_refs, depth + 1, active_objs, active_arrs);
}

void build_hash_context(const Z4V& header_val, Amf3Context& ctx, Z4L& limits) {
    std::vector<uint8_t> bytes;
    if (header_val.kind == Z4K::ByteArray) {
        bytes = header_val.bytes;
    } else {
        return;
    }
    if (bytes.empty()) return;
    try_decompress_zlib(bytes);
    Reader sub(bytes.data(), bytes.size(), limits);
    while (!sub.eof()) {
        const size_t before = sub.pos();
        sub.read_amf3_value();
        if (sub.pos() == before) break;
    }
    ctx = sub.capture_amf3_context();
}

X7D2 decode_remoting(const uint8_t* data, size_t len, Z4L& limits) {
    X7D2 result{};
    if (len < 4) {
        return result;
    }

    Reader r(data, len, limits);
    (void)r.read_u16();
    const uint16_t header_count = r.read_u16();

    std::vector<Z4V> header_values;
    std::vector<Z4S> header_ref_snapshots;
    Amf3Context hash_context;
    header_values.reserve(header_count);
    header_ref_snapshots.reserve(header_count);

    for (uint16_t hi = 0; hi < header_count && !r.eof(); ++hi) {
        const std::string name = r.read_amf0_string();
        (void)r.read_u8();
        const int32_t hlen = r.read_i32_be();

        Z4V hval = read_length_payload(r, len, hlen, limits);
        if (name == "hashContent") {
            build_hash_context(hval, hash_context, limits);
        }
        header_values.push_back(std::move(hval));
        header_ref_snapshots.push_back(r.snapshot());
    }

    const uint16_t body_count = r.eof() ? 0 : r.read_u16();

    std::vector<Z4V> body_values;
    std::vector<Z4S> body_ref_snapshots;
    body_values.reserve(body_count);
    body_ref_snapshots.reserve(body_count);

    for (uint16_t bi = 0; bi < body_count && !r.eof(); ++bi) {
        r.clear_body_context();

        const std::string target = r.read_amf0_string();
        (void)r.read_amf0_string();
        const int32_t blen = r.read_i32_be();

        Z4V payload = read_length_payload(r, len, blen, limits, &target);
        body_values.push_back(std::move(payload));
        body_ref_snapshots.push_back(r.snapshot());
    }

    for (size_t hi = 0; hi < header_values.size(); ++hi) {
        resolve_value_refs(
            header_values[hi],
            header_ref_snapshots[hi].obj_refs,
            header_ref_snapshots[hi].array_refs,
            0);
    }
    for (size_t bi = 0; bi < body_values.size(); ++bi) {
        resolve_value_refs(
            body_values[bi],
            body_ref_snapshots[bi].obj_refs,
            body_ref_snapshots[bi].array_refs,
            0);
    }

    if (!body_values.empty()) {
        std::ostringstream js;
        z9jc(body_values[0], js, 0);
        result.json = js.str();
    }
    return result;
}

}  // namespace

X7D2 x7k9_m9p1(const std::vector<uint8_t>& data) {
    X7D2 result{};
    if (data.empty()) {
        return result;
    }

    Z4L limits;
    if (data[0] == 0x00 && data.size() >= 2 && (data[1] == 0x03 || data[1] == 0x00)) {
        return decode_remoting(data.data(), data.size(), limits);
    }

    Reader rd(data.data(), data.size(), limits);
    Z4V value;
    if (data[0] == 0x11 || (data[0] >= 0x01 && data[0] <= 0x0e)) {
        value = data[0] == 0x11 ? rd.read_remoting_payload() : rd.read_amf3_value();
    } else {
        value = rd.read_remoting_payload();
    }

    std::ostringstream js;
    z9jc(value, js, 0);
    result.json = js.str();
    return result;
}

}  // namespace x7k2::q9m4
