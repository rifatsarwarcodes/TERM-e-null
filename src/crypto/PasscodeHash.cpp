#include "crypto/PasscodeHash.hpp"

#include "common/Strings.hpp"
#include "crypto/Sha256.hpp"

#include <random>

namespace ovc::crypto {

namespace {

constexpr std::string_view Scheme = "sha256-iter";

std::string randomSaltHex()
{
    std::random_device device;
    std::uint8_t salt[16];
    for (auto& byte : salt)
        byte = static_cast<std::uint8_t>(device() & 0xFF);
    return strings::toHex(salt, sizeof salt);
}

// Comparison that does not stop at the first differing character.
bool equalsConstantTime(std::string_view a, std::string_view b)
{
    if (a.size() != b.size())
        return false;
    unsigned char difference = 0;
    for (std::size_t i = 0; i < a.size(); ++i)
        difference |= static_cast<unsigned char>(a[i] ^ b[i]);
    return difference == 0;
}

} // namespace

PasscodeHash::PasscodeHash(int iterations, std::string saltHex, std::string hashHex)
    : iterations_(iterations)
    , saltHex_(std::move(saltHex))
    , hashHex_(std::move(hashHex))
{
}

PasscodeHash PasscodeHash::create(std::string_view passcode, int iterations)
{
    std::string salt = randomSaltHex();
    std::string hash = compute(passcode, salt, iterations);
    return PasscodeHash(iterations, std::move(salt), std::move(hash));
}

std::optional<PasscodeHash> PasscodeHash::parse(std::string_view encoded)
{
    const auto parts = strings::split(strings::trim(encoded), ':');
    if (parts.size() != 4 || parts[0] != Scheme)
        return std::nullopt;
    std::uint64_t iterations = 0;
    if (!strings::parseUnsigned(parts[1], iterations) || iterations == 0 || iterations > 10'000'000)
        return std::nullopt;
    if (parts[2].empty() || parts[3].size() != 64)
        return std::nullopt;
    return PasscodeHash(static_cast<int>(iterations), parts[2], parts[3]);
}

bool PasscodeHash::matches(std::string_view passcode) const
{
    return equalsConstantTime(compute(passcode, saltHex_, iterations_), hashHex_);
}

std::string PasscodeHash::encode() const
{
    return std::string(Scheme) + ":" + std::to_string(iterations_) + ":" + saltHex_ + ":" + hashHex_;
}

std::string PasscodeHash::compute(std::string_view passcode, std::string_view saltHex, int iterations)
{
    std::string current = Sha256::hex(std::string(saltHex) + std::string(passcode));
    for (int i = 1; i < iterations; ++i)
        current = Sha256::hex(current + std::string(saltHex));
    return current;
}

} // namespace ovc::crypto
