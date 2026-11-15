#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace ovc::crypto {

// Stores the examiner passcode as a salted SHA-256 hash that is repeated many
// times, so the plain passcode never appears in the exam package.
//
// Encoded form: "sha256-iter:<iterations>:<salt hex>:<hash hex>"
class PasscodeHash {
public:
    static constexpr int DefaultIterations = 10000;

    static PasscodeHash create(std::string_view passcode, int iterations = DefaultIterations);
    static std::optional<PasscodeHash> parse(std::string_view encoded);

    bool matches(std::string_view passcode) const;
    std::string encode() const;

private:
    PasscodeHash(int iterations, std::string saltHex, std::string hashHex);

    static std::string compute(std::string_view passcode, std::string_view saltHex, int iterations);

    int iterations_;
    std::string saltHex_;
    std::string hashHex_;
};

} // namespace ovc::crypto
