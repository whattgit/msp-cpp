#pragma once

#include <cstdint>
#include <string>

namespace x7k2::q9m4::detail {

constexpr uint8_t kSxK = 0xC3u;

inline std::string x7_sxd(const uint8_t* data, size_t length) {
    std::string out(length, '\0');
    uint8_t roll = kSxK;
    for (size_t i = 0; i < length; ++i) {
        roll = static_cast<uint8_t>((roll * 37u + static_cast<uint8_t>(i + 1)) ^ kSxK);
        out[i] = static_cast<char>(data[i] ^ roll);
    }
    return out;
}

template <size_t N>
inline std::string x7_sxs(const uint8_t (&encoded)[N]) {
    return x7_sxd(encoded, N);
}

}  // namespace x7k2::q9m4::detail
