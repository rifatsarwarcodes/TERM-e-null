#pragma once

#include "common/Time.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace ovc::evidence {

// Written when a session ends. The receipt code is shown to the student and
// copied onto the attendance sheet; it is derived from the last hashes of
// both logs, so rewriting either log afterwards changes the code.
struct Seal {
    std::string eventsHead;
    std::string journalHead;
    std::string receipt;
    timeutil::Millis sealedAt = 0;

    std::string encode() const;
    static std::optional<Seal> decode(const std::string& text);

    bool write(const std::filesystem::path& path) const;
    static std::optional<Seal> read(const std::filesystem::path& path);
};

// "7F3A-91C2-B0": the first ten hex digits of SHA-256(eventsHead + journalHead).
std::string receiptCode(const std::string& eventsHead, const std::string& journalHead);

// Compares receipts while ignoring case, spaces and dashes.
bool receiptsMatch(const std::string& a, const std::string& b);

} // namespace ovc::evidence
