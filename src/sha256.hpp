// sha256.hpp — compact SHA-256 for on-device media integrity checks.
// Classic public-domain reference structure (Brad Conte style), FIPS 180-4.
#pragma once

#include <cstdint>
#include <cstddef>
#include <string>

class Sha256 {
public:
    Sha256();

    void update(const uint8_t* data, size_t len);
    void final(uint8_t out[32]);

    // Hex string, lowercase.
    static std::string hex(const uint8_t digest[32]);

    // One-shot helpers.
    static std::string hexOfBuffer(const uint8_t* data, size_t len);
    static std::string hexOfString(const std::string& s);

    // Klaxon "quick" integrity digest: hashes the first prefixBytes of a
    // file plus an 8-byte little-endian file size trailer.  Catches swapped
    // or wrong files in milliseconds instead of hashing gigabytes.
    // Returns false if the file cannot be opened.
    static bool quickFile(const char* path, size_t prefixBytes,
                          std::string& outHex, uint64_t* outSize = nullptr);

    // Full streaming hash of a file.  Slow on large media; use for small
    // files or when the user opts into full verification.
    static bool file(const char* path, std::string& outHex);

private:
    void transform(const uint8_t chunk[64]);

    uint32_t state_[8];
    uint64_t bits_;
    uint8_t  block_[64];
    size_t   fill_;
};
