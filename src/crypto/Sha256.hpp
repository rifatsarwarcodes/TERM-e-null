#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace ovc::crypto {

// SHA-256 as specified in FIPS 180-4.
//
//   Sha256 hasher;
//   hasher.update("abc");
//   std::string hex = hasher.hexDigest();
//
// After digest() the object is finished; call reset() to reuse it.
class Sha256 {
public:
    static constexpr std::size_t DigestSize = 32;
    using Digest = std::array<std::uint8_t, DigestSize>;

    Sha256();

    void reset();
    void update(const void* data, std::size_t length);
    void update(std::string_view text);

    Digest digest();
    std::string hexDigest();

    // One-shot helper: lowercase hex digest of the text.
    static std::string hex(std::string_view text);

private:
    void processBlock(const std::uint8_t* block);

    std::array<std::uint32_t, 8> state_{};
    std::array<std::uint8_t, 64> buffer_{};
    std::size_t bufferLength_ = 0;
    std::uint64_t totalBytes_ = 0;
};

} // namespace ovc::crypto
