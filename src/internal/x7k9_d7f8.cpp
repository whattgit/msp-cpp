#include "internal/x7k9_core.hpp"

#include <Windows.h>
#include <Wincrypt.h>

#include <cctype>
#include <iomanip>
#include <random>
#include <sstream>
#include <stdexcept>

#pragma comment(lib, "crypt32.lib")

namespace x7k2::q9m4 {
namespace {

std::string hash_hex(ALG_ID alg, const uint8_t* data, size_t size) {
    HCRYPTPROV provider = 0;
    HCRYPTHASH hash = 0;
    if (!CryptAcquireContextA(&provider, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
        throw std::runtime_error("CryptAcquireContext failed");
    }
    if (!CryptCreateHash(provider, alg, 0, 0, &hash)) {
        CryptReleaseContext(provider, 0);
        throw std::runtime_error("CryptCreateHash failed");
    }
    if (!CryptHashData(hash, data, static_cast<DWORD>(size), 0)) {
        CryptDestroyHash(hash);
        CryptReleaseContext(provider, 0);
        throw std::runtime_error("CryptHashData failed");
    }
    DWORD hash_size = 0;
    DWORD hash_size_len = sizeof(hash_size);
    CryptGetHashParam(hash, HP_HASHSIZE, reinterpret_cast<BYTE*>(&hash_size), &hash_size_len, 0);
    std::vector<uint8_t> buffer(hash_size);
    CryptGetHashParam(hash, HP_HASHVAL, buffer.data(), &hash_size, 0);
    CryptDestroyHash(hash);
    CryptReleaseContext(provider, 0);
    return bytes_to_hex(buffer);
}

}  // namespace

std::string md5_hex(const std::string& data) {
    return hash_hex(CALG_MD5, reinterpret_cast<const uint8_t*>(data.data()), data.size());
}

std::string sha1_hex(const std::string& data) {
    return hash_hex(CALG_SHA1, reinterpret_cast<const uint8_t*>(data.data()), data.size());
}

std::string bytes_to_hex(const std::vector<uint8_t>& data) {
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (const uint8_t byte : data) {
        out << std::setw(2) << static_cast<int>(byte);
    }
    return out.str();
}

std::vector<uint8_t> base64url_decode(const std::string& input) {
    std::string normalized = input;
    for (char& ch : normalized) {
        if (ch == '-') {
            ch = '+';
        } else if (ch == '_') {
            ch = '/';
        }
    }
    while (normalized.size() % 4 != 0) {
        normalized.push_back('=');
    }
    DWORD decoded_size = 0;
    if (!CryptStringToBinaryA(
            normalized.c_str(),
            static_cast<DWORD>(normalized.size()),
            CRYPT_STRING_BASE64,
            nullptr,
            &decoded_size,
            nullptr,
            nullptr)) {
        throw std::runtime_error("CryptStringToBinary size failed");
    }
    std::vector<uint8_t> out(decoded_size);
    if (!CryptStringToBinaryA(
            normalized.c_str(),
            static_cast<DWORD>(normalized.size()),
            CRYPT_STRING_BASE64,
            out.data(),
            &decoded_size,
            nullptr,
            nullptr)) {
        throw std::runtime_error("CryptStringToBinary failed");
    }
    out.resize(decoded_size);
    return out;
}

std::string random_hex(size_t byte_count) {
    std::vector<uint8_t> bytes(byte_count);
    if (!CryptGenRandom(0, static_cast<DWORD>(bytes.size()), bytes.data())) {
        HCRYPTPROV provider = 0;
        CryptAcquireContextA(&provider, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT);
        CryptGenRandom(provider, static_cast<DWORD>(bytes.size()), bytes.data());
        CryptReleaseContext(provider, 0);
    }
    return bytes_to_hex(bytes);
}

std::string to_lower(std::string value) {
    for (char& ch : value) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return value;
}

std::string url_encode(const std::string& value) {
    static const char* hex = "0123456789ABCDEF";
    std::string out;
    for (unsigned char ch : value) {
        if (std::isalnum(ch) || ch == '-' || ch == '_' || ch == '.' || ch == '~') {
            out.push_back(static_cast<char>(ch));
        } else {
            out.push_back('%');
            out.push_back(hex[ch >> 4]);
            out.push_back(hex[ch & 0x0F]);
        }
    }
    return out;
}

}  // namespace x7k2::q9m4
