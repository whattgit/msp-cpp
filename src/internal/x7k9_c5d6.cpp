#include "internal/x7k9_core.hpp"

extern "C" {
#include "miniz.h"
}

#include <cstring>

namespace x7k2::q9m4 {
namespace {

bool looks_like_amf(const std::vector<uint8_t>& body) {
    if (body.size() < 2) {
        return false;
    }
    return body[0] == 0x00 || body[0] == 0x03 || body[0] == 0x11 || body[0] == 0x0A;
}

bool try_zlib(std::vector<uint8_t>& body) {
    if (body.size() < 2 || body[0] != 0x78) {
        return false;
    }
    mz_ulong dest_len = static_cast<mz_ulong>(body.size() * 16 + 4096);
    std::vector<uint8_t> dest(dest_len);
    if (mz_uncompress(dest.data(), &dest_len, body.data(), static_cast<mz_ulong>(body.size())) != MZ_OK) {
        return false;
    }
    dest.resize(dest_len);
    body = std::move(dest);
    return true;
}

bool try_gzip(std::vector<uint8_t>& body) {
    if (body.size() < 12 || body[0] != 0x1F || body[1] != 0x8B) {
        return false;
    }
    size_t out_len = 0;
    void* heap = tinfl_decompress_mem_to_heap(body.data(), body.size(), &out_len, 0);
    if (!heap || out_len == 0) {
        if (heap) {
            mz_free(heap);
        }
        return false;
    }
    std::vector<uint8_t> dest(out_len);
    std::memcpy(dest.data(), heap, out_len);
    mz_free(heap);
    body = std::move(dest);
    return true;
}

}  // namespace

std::vector<uint8_t> x7k9_z1d3(std::vector<uint8_t> body) {
    if (looks_like_amf(body)) {
        return body;
    }
    if (try_gzip(body) && looks_like_amf(body)) {
        return body;
    }
    if (try_zlib(body) && looks_like_amf(body)) {
        return body;
    }
    return body;
}

}  // namespace x7k2::q9m4
