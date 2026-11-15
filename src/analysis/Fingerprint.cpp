#include "analysis/Fingerprint.hpp"

#include <algorithm>
#include <cctype>

namespace ovc::analysis {

namespace {

// Arithmetic is modulo 2^64 through unsigned overflow.
constexpr std::uint64_t Base = 1099511628211ULL;

} // namespace

Fingerprinter::Fingerprinter(std::size_t k, std::size_t window)
    : k_(k == 0 ? 1 : k)
    , window_(window == 0 ? 1 : window)
{
}

std::string Fingerprinter::normalise(std::string_view code)
{
    std::string out;
    out.reserve(code.size());
    std::size_t i = 0;
    while (i < code.size()) {
        if (code[i] == '/' && i + 1 < code.size() && code[i + 1] == '/') {
            while (i < code.size() && code[i] != '\n')
                ++i;
            continue;
        }
        if (code[i] == '/' && i + 1 < code.size() && code[i + 1] == '*') {
            const std::size_t end = code.find("*/", i + 2);
            i = end == std::string_view::npos ? code.size() : end + 2;
            continue;
        }
        if (!std::isspace(static_cast<unsigned char>(code[i])))
            out.push_back(code[i]);
        ++i;
    }
    return out;
}

std::vector<std::uint64_t> Fingerprinter::fingerprints(std::string_view code) const
{
    const std::string text = normalise(code);
    std::vector<std::uint64_t> selected;
    if (text.size() < k_)
        return selected;

    // Rolling hash of every k-gram: h = c0*B^(k-1) + ... + c(k-1).
    std::uint64_t highPower = 1;
    for (std::size_t i = 1; i < k_; ++i)
        highPower *= Base;

    std::vector<std::uint64_t> hashes;
    hashes.reserve(text.size() - k_ + 1);
    std::uint64_t hash = 0;
    for (std::size_t i = 0; i < k_; ++i)
        hash = hash * Base + static_cast<unsigned char>(text[i]);
    hashes.push_back(hash);
    for (std::size_t i = k_; i < text.size(); ++i) {
        hash -= static_cast<unsigned char>(text[i - k_]) * highPower;
        hash = hash * Base + static_cast<unsigned char>(text[i]);
        hashes.push_back(hash);
    }

    // Winnowing: keep the minimum of every window, once per position.
    if (hashes.size() <= window_) {
        std::uint64_t smallest = hashes.front();
        for (std::uint64_t h : hashes)
            smallest = std::min(smallest, h);
        selected.push_back(smallest);
        return selected;
    }
    std::size_t lastPicked = static_cast<std::size_t>(-1);
    for (std::size_t start = 0; start + window_ <= hashes.size(); ++start) {
        std::size_t best = start;
        for (std::size_t j = start; j < start + window_; ++j)
            if (hashes[j] <= hashes[best]) // rightmost minimum
                best = j;
        if (best != lastPicked) {
            selected.push_back(hashes[best]);
            lastPicked = best;
        }
    }
    return selected;
}

FingerprintIndex::FingerprintIndex(Fingerprinter fingerprinter)
    : fingerprinter_(fingerprinter)
{
}

void FingerprintIndex::add(std::string_view text)
{
    for (std::uint64_t hash : fingerprinter_.fingerprints(text))
        hashes_.insert(hash);
    corpus_ += Fingerprinter::normalise(text);
    corpus_ += '\x01'; // keeps separate texts from joining
}

void FingerprintIndex::clear()
{
    hashes_.clear();
    corpus_.clear();
}

double FingerprintIndex::coverage(std::string_view text) const
{
    const std::vector<std::uint64_t> prints = fingerprinter_.fingerprints(text);
    if (prints.empty()) {
        const std::string normalised = Fingerprinter::normalise(text);
        if (normalised.empty())
            return 1.0; // only whitespace or comments
        return corpus_.find(normalised) != std::string::npos ? 1.0 : 0.0;
    }
    std::size_t found = 0;
    for (std::uint64_t hash : prints)
        if (hashes_.count(hash) != 0)
            ++found;
    return static_cast<double>(found) / static_cast<double>(prints.size());
}

} // namespace ovc::analysis
