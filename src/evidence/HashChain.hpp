#pragma once

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ovc::evidence {

namespace fs = std::filesystem;

// A hash-chained, append-only text log. Every line is
//
//     <body>|h=<sha256(previous hash + "\n" + body)>
//
// so changing or removing any line breaks every hash after it. The first
// line chains from a hash of 64 zeros.
namespace chain {

inline const std::string GenesisHash(64, '0');
std::string linkHash(const std::string& previousHash, std::string_view body);

} // namespace chain

class ChainedLogWriter {
public:
    ChainedLogWriter() = default;

    // Opens the log for appending. An existing log is continued from its
    // last stored hash, so an earlier break stays visible where it is. A
    // line cut off by a crash is removed; its size is reported by
    // droppedBytes() so the caller can record it.
    bool open(const fs::path& path);
    std::size_t droppedBytes() const { return droppedBytes_; }
    bool isOpen() const { return out_.is_open(); }

    // Writes and flushes one line; returns the new head hash.
    const std::string& append(std::string_view body);

    const std::string& headHash() const { return head_; }
    std::size_t lineCount() const { return lines_; }
    // The last body written, e.g. to continue sequence numbers on resume.
    const std::string& lastBody() const { return lastBody_; }

private:
    std::ofstream out_;
    std::string head_ = chain::GenesisHash;
    std::string lastBody_;
    std::size_t lines_ = 0;
    std::size_t droppedBytes_ = 0;
};

// Result of reading and checking a chained log.
struct ChainReport {
    std::vector<std::string> bodies;      // every readable line, in order
    bool fileFound = false;
    bool intact = true;                   // all hashes match
    std::optional<std::size_t> firstBrokenLine; // 1-based
    bool truncatedTail = false;           // last line cut off (crash while writing)
    std::size_t completeBytes = 0;        // file size up to the last complete line
    std::size_t truncatedBytes = 0;       // size of the cut-off part, if any
    std::string headHash = chain::GenesisHash; // last stored hash

    bool usable() const { return fileFound; }
};

ChainReport readChain(const fs::path& path);

} // namespace ovc::evidence
