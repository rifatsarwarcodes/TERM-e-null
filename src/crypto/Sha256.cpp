#include "crypto/Sha256.hpp"

#include "common/Strings.hpp"

#include <algorithm>
#include <cstring>

namespace ovc::crypto {

namespace {

// First 32 bits of the fractional parts of the cube roots of the first 64 primes.
constexpr std::uint32_t RoundConstants[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

// First 32 bits of the fractional parts of the square roots of the first 8 primes.
constexpr std::uint32_t InitialState[8] = {
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19,
};

constexpr std::uint32_t rotateRight(std::uint32_t value, unsigned bits)
{
    return (value >> bits) | (value << (32 - bits));
}

std::uint32_t readBigEndian32(const std::uint8_t* bytes)
{
    return (static_cast<std::uint32_t>(bytes[0]) << 24) | (static_cast<std::uint32_t>(bytes[1]) << 16)
        | (static_cast<std::uint32_t>(bytes[2]) << 8) | static_cast<std::uint32_t>(bytes[3]);
}

} // namespace

Sha256::Sha256()
{
    reset();
}

void Sha256::reset()
{
    std::memcpy(state_.data(), InitialState, sizeof InitialState);
    bufferLength_ = 0;
    totalBytes_ = 0;
}

void Sha256::update(const void* data, std::size_t length)
{
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    totalBytes_ += length;

    while (length > 0) {
        const std::size_t take = std::min(length, buffer_.size() - bufferLength_);
        std::memcpy(buffer_.data() + bufferLength_, bytes, take);
        bufferLength_ += take;
        bytes += take;
        length -= take;
        if (bufferLength_ == buffer_.size()) {
            processBlock(buffer_.data());
            bufferLength_ = 0;
        }
    }
}

void Sha256::update(std::string_view text)
{
    update(text.data(), text.size());
}

Sha256::Digest Sha256::digest()
{
    const std::uint64_t totalBits = totalBytes_ * 8;

    // Padding: a single 1 bit, zeros, then the message length in bits as a
    // 64-bit big-endian number, so the total is a multiple of 64 bytes.
    const std::uint8_t one = 0x80;
    update(&one, 1);
    const std::uint8_t zero = 0x00;
    while (bufferLength_ != 56)
        update(&zero, 1);

    std::uint8_t lengthBytes[8];
    for (int i = 0; i < 8; ++i)
        lengthBytes[i] = static_cast<std::uint8_t>(totalBits >> (56 - 8 * i));
    update(lengthBytes, sizeof lengthBytes);

    Digest result{};
    for (std::size_t i = 0; i < state_.size(); ++i) {
        result[i * 4 + 0] = static_cast<std::uint8_t>(state_[i] >> 24);
        result[i * 4 + 1] = static_cast<std::uint8_t>(state_[i] >> 16);
        result[i * 4 + 2] = static_cast<std::uint8_t>(state_[i] >> 8);
        result[i * 4 + 3] = static_cast<std::uint8_t>(state_[i]);
    }
    return result;
}

std::string Sha256::hexDigest()
{
    const Digest bytes = digest();
    return strings::toHex(bytes.data(), bytes.size());
}

std::string Sha256::hex(std::string_view text)
{
    Sha256 hasher;
    hasher.update(text);
    return hasher.hexDigest();
}

void Sha256::processBlock(const std::uint8_t* block)
{
    std::uint32_t schedule[64];
    for (int i = 0; i < 16; ++i)
        schedule[i] = readBigEndian32(block + i * 4);
    for (int i = 16; i < 64; ++i) {
        const std::uint32_t s0 = rotateRight(schedule[i - 15], 7) ^ rotateRight(schedule[i - 15], 18)
            ^ (schedule[i - 15] >> 3);
        const std::uint32_t s1 = rotateRight(schedule[i - 2], 17) ^ rotateRight(schedule[i - 2], 19)
            ^ (schedule[i - 2] >> 10);
        schedule[i] = schedule[i - 16] + s0 + schedule[i - 7] + s1;
    }

    std::uint32_t a = state_[0], b = state_[1], c = state_[2], d = state_[3];
    std::uint32_t e = state_[4], f = state_[5], g = state_[6], h = state_[7];

    for (int i = 0; i < 64; ++i) {
        const std::uint32_t sum1 = rotateRight(e, 6) ^ rotateRight(e, 11) ^ rotateRight(e, 25);
        const std::uint32_t choose = (e & f) ^ (~e & g);
        const std::uint32_t temp1 = h + sum1 + choose + RoundConstants[i] + schedule[i];
        const std::uint32_t sum0 = rotateRight(a, 2) ^ rotateRight(a, 13) ^ rotateRight(a, 22);
        const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        const std::uint32_t temp2 = sum0 + majority;

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    state_[0] += a;
    state_[1] += b;
    state_[2] += c;
    state_[3] += d;
    state_[4] += e;
    state_[5] += f;
    state_[6] += g;
    state_[7] += h;
}

} // namespace ovc::crypto
