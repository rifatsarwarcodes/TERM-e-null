#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace ovc::analysis {

// Document fingerprints with a Karp-Rabin rolling hash and winnowing
// (Schleimer, Wilkerson and Aiken, 2003).
//
// Code is first normalised (comments and all whitespace removed), so moving
// code around or re-indenting it does not hide where it came from. Every run
// of k characters is hashed with a rolling hash; from each window of w
// consecutive hashes the smallest is kept. Two texts that share a long enough
// piece of code are guaranteed to share a fingerprint.
class Fingerprinter {
public:
    explicit Fingerprinter(std::size_t k = 12, std::size_t window = 6);

    static std::string normalise(std::string_view code);
    std::vector<std::uint64_t> fingerprints(std::string_view code) const;

    // Normalised texts shorter than this have no fingerprints.
    std::size_t minimumLength() const { return k_; }

private:
    std::size_t k_;
    std::size_t window_;
};

// A set of fingerprints from reference texts, to ask how much of a new text
// is covered by them.
class FingerprintIndex {
public:
    explicit FingerprintIndex(Fingerprinter fingerprinter = Fingerprinter());

    void add(std::string_view text);
    void clear();

    // Share of the text's fingerprints found in the index, 0.0 to 1.0. Short
    // texts without fingerprints are checked as normalised substrings.
    double coverage(std::string_view text) const;

private:
    Fingerprinter fingerprinter_;
    std::unordered_set<std::uint64_t> hashes_;
    std::string corpus_; // normalised reference text, for short pastes
};

} // namespace ovc::analysis
