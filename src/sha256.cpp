#include "sha256.hpp"

#include <cstdio>
#include <cstring>

namespace {
const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
    0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
    0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
    0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
    0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

inline uint32_t rotr(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }
inline uint32_t ch(uint32_t x, uint32_t y, uint32_t z)  { return (x & y) ^ (~x & z); }
inline uint32_t maj(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); }
inline uint32_t bigSigma0(uint32_t x) { return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22); }
inline uint32_t bigSigma1(uint32_t x) { return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25); }
inline uint32_t litSigma0(uint32_t x) { return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3); }
inline uint32_t litSigma1(uint32_t x) { return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10); }
}  // namespace

Sha256::Sha256() {
    state_[0] = 0x6a09e667; state_[1] = 0xbb67ae85;
    state_[2] = 0x3c6ef372; state_[3] = 0xa54ff53a;
    state_[4] = 0x510e527f; state_[5] = 0x9b05688c;
    state_[6] = 0x1f83d9ab; state_[7] = 0x5be0cd19;
    bits_ = 0;
    fill_ = 0;
    memset(block_, 0, sizeof(block_));
}

void Sha256::transform(const uint8_t* chunk) {
    uint32_t w[64];
    for (int t = 0; t < 16; t++) {
        w[t] = (uint32_t(chunk[t * 4 + 0]) << 24) |
               (uint32_t(chunk[t * 4 + 1]) << 16) |
               (uint32_t(chunk[t * 4 + 2]) << 8)  |
               (uint32_t(chunk[t * 4 + 3]));
    }
    for (int t = 16; t < 64; t++) {
        w[t] = litSigma1(w[t - 2]) + w[t - 7] + litSigma0(w[t - 15]) + w[t - 16];
    }

    uint32_t a = state_[0], b = state_[1], c = state_[2], d = state_[3];
    uint32_t e = state_[4], f = state_[5], g = state_[6], h = state_[7];

    for (int t = 0; t < 64; t++) {
        uint32_t t1 = h + bigSigma1(e) + ch(e, f, g) + K[t] + w[t];
        uint32_t t2 = bigSigma0(a) + maj(a, b, c);
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }

    state_[0] += a; state_[1] += b; state_[2] += c; state_[3] += d;
    state_[4] += e; state_[5] += f; state_[6] += g; state_[7] += h;
}

void Sha256::update(const uint8_t* data, size_t len) {
    bits_ += (uint64_t)len * 8;
    while (len > 0) {
        size_t take = 64 - fill_;
        if (take > len) take = len;
        memcpy(block_ + fill_, data, take);
        fill_ += take;
        data += take;
        len -= take;
        if (fill_ == 64) {
            transform(block_);
            fill_ = 0;
        }
    }
}

void Sha256::final(uint8_t out[32]) {
    uint64_t bits = bits_;
    uint8_t pad = 0x80;
    update(&pad, 1);
    uint8_t zero = 0;
    while (fill_ != 56) {
        update(&zero, 1);
    }
    uint8_t lenBytes[8];
    for (int i = 0; i < 8; i++) {
        lenBytes[i] = uint8_t(bits >> (56 - i * 8));
    }
    // Write length without touching bits_ again.
    memcpy(block_ + 56, lenBytes, 8);
    transform(block_);
    fill_ = 0;

    for (int i = 0; i < 8; i++) {
        out[i * 4 + 0] = uint8_t(state_[i] >> 24);
        out[i * 4 + 1] = uint8_t(state_[i] >> 16);
        out[i * 4 + 2] = uint8_t(state_[i] >> 8);
        out[i * 4 + 3] = uint8_t(state_[i]);
    }
}

std::string Sha256::hex(const uint8_t digest[32]) {
    static const char* digits = "0123456789abcdef";
    std::string s;
    s.reserve(64);
    for (int i = 0; i < 32; i++) {
        s += digits[digest[i] >> 4];
        s += digits[digest[i] & 0xF];
    }
    return s;
}

std::string Sha256::hexOfBuffer(const uint8_t* data, size_t len) {
    Sha256 h;
    h.update(data, len);
    uint8_t d[32];
    h.final(d);
    return hex(d);
}

std::string Sha256::hexOfString(const std::string& s) {
    return hexOfBuffer(reinterpret_cast<const uint8_t*>(s.data()), s.size());
}

bool Sha256::quickFile(const char* path, size_t prefixBytes,
                       std::string& outHex, uint64_t* outSize) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;

    Sha256 h;
    uint8_t buf[32768];
    size_t remaining = prefixBytes;
    size_t total = 0;
    while (remaining > 0) {
        size_t want = remaining < sizeof(buf) ? remaining : sizeof(buf);
        size_t got = fread(buf, 1, want, f);
        if (got == 0) break;
        h.update(buf, got);
        remaining -= got;
        total += got;
    }
    fseek(f, 0, SEEK_END);
    // wut/newlib: ftell is fine here (largest media file is ~0.5 GB).
    long sz64 = ftell(f);
    fclose(f);

    uint8_t trailer[8];
    uint64_t sz = (uint64_t)sz64;
    for (int i = 0; i < 8; i++) trailer[i] = uint8_t(sz >> (i * 8));
    h.update(trailer, 8);

    uint8_t digest[32];
    h.final(digest);
    outHex = hex(digest);
    if (outSize) *outSize = sz;
    (void)total;
    return true;
}

bool Sha256::file(const char* path, std::string& outHex) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;

    Sha256 h;
    uint8_t buf[32768];
    size_t got;
    while ((got = fread(buf, 1, sizeof(buf), f)) > 0) {
        h.update(buf, got);
    }
    fclose(f);

    uint8_t digest[32];
    h.final(digest);
    outHex = hex(digest);
    return true;
}
