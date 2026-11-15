#include "evidence/Seal.hpp"

#include "common/Files.hpp"
#include "common/Strings.hpp"
#include "crypto/Sha256.hpp"

#include <cctype>

namespace ovc::evidence {

std::string Seal::encode() const
{
    return "events_head=" + eventsHead + "\njournal_head=" + journalHead + "\nreceipt=" + receipt
        + "\nsealed_at=" + std::to_string(sealedAt) + "\n";
}

std::optional<Seal> Seal::decode(const std::string& text)
{
    Seal seal;
    for (const auto& line : strings::splitLines(text)) {
        const std::size_t equals = line.find('=');
        if (equals == std::string::npos)
            continue;
        const std::string key = line.substr(0, equals);
        const std::string value = strings::trim(line.substr(equals + 1));
        long long number = 0;
        if (key == "events_head") seal.eventsHead = value;
        else if (key == "journal_head") seal.journalHead = value;
        else if (key == "receipt") seal.receipt = value;
        else if (key == "sealed_at" && strings::parseInt(value, number)) seal.sealedAt = number;
    }
    if (seal.eventsHead.empty() || seal.receipt.empty())
        return std::nullopt;
    return seal;
}

bool Seal::write(const std::filesystem::path& path) const
{
    return files::writeAtomically(path, encode());
}

std::optional<Seal> Seal::read(const std::filesystem::path& path)
{
    const auto text = files::read(path);
    if (!text)
        return std::nullopt;
    return decode(*text);
}

std::string receiptCode(const std::string& eventsHead, const std::string& journalHead)
{
    const std::string digits = strings::toUpper(crypto::Sha256::hex(eventsHead + journalHead).substr(0, 10));
    return digits.substr(0, 4) + "-" + digits.substr(4, 4) + "-" + digits.substr(8, 2);
}

bool receiptsMatch(const std::string& a, const std::string& b)
{
    auto normalise = [](const std::string& text) {
        std::string result;
        for (char c : text)
            if (std::isalnum(static_cast<unsigned char>(c)))
                result.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
        return result;
    };
    const std::string left = normalise(a);
    return !left.empty() && left == normalise(b);
}

} // namespace ovc::evidence
