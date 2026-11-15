#include "evidence/HashChain.hpp"

#include "common/Files.hpp"
#include "crypto/Sha256.hpp"

namespace ovc::evidence {

namespace {

constexpr std::string_view HashMarker = "|h=";

struct SplitLine {
    std::string body;
    std::string hash;
};

std::optional<SplitLine> splitLine(const std::string& line)
{
    const std::size_t marker = line.rfind(HashMarker);
    if (marker == std::string::npos)
        return std::nullopt;
    SplitLine split{line.substr(0, marker), line.substr(marker + HashMarker.size())};
    if (split.hash.size() != 64)
        return std::nullopt;
    return split;
}

bool endsWithoutNewline(const fs::path& path)
{
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in || in.tellg() <= 0)
        return false;
    in.seekg(-1, std::ios::end);
    char last = '\n';
    in.get(last);
    return last != '\n';
}

} // namespace

std::string chain::linkHash(const std::string& previousHash, std::string_view body)
{
    crypto::Sha256 hasher;
    hasher.update(previousHash);
    hasher.update("\n");
    hasher.update(body);
    return hasher.hexDigest();
}

bool ChainedLogWriter::open(const fs::path& path)
{
    const ChainReport existing = readChain(path);
    head_ = existing.headHash;
    lines_ = existing.bodies.size();
    lastBody_ = existing.bodies.empty() ? std::string() : existing.bodies.back();

    droppedBytes_ = 0;
    if (existing.truncatedTail) {
        std::error_code error;
        fs::resize_file(path, existing.completeBytes, error);
        if (!error)
            droppedBytes_ = existing.truncatedBytes;
    }
    const bool needsNewline = endsWithoutNewline(path);
    out_.open(path, std::ios::binary | std::ios::app);
    if (out_ && needsNewline)
        out_ << "\n";
    return out_.is_open();
}

const std::string& ChainedLogWriter::append(std::string_view body)
{
    head_ = chain::linkHash(head_, body);
    out_ << body << HashMarker << head_ << "\n";
    out_.flush();
    lastBody_ = std::string(body);
    ++lines_;
    return head_;
}

ChainReport readChain(const fs::path& path)
{
    ChainReport report;
    const auto content = files::read(path);
    if (!content)
        return report;
    report.fileFound = true;

    std::string previous = chain::GenesisHash;
    std::size_t lineNumber = 0;
    std::size_t start = 0;
    while (start < content->size()) {
        std::size_t end = content->find('\n', start);
        const bool complete = end != std::string::npos;
        if (!complete)
            end = content->size();
        const std::string line = content->substr(start, end - start);
        start = end + 1;
        if (complete)
            report.completeBytes = start;
        if (line.empty())
            continue;
        ++lineNumber;

        const auto split = splitLine(line);
        if (!split) {
            if (!complete) {
                report.truncatedTail = true; // a half-written last line
                report.truncatedBytes = line.size();
                break;
            }
            if (!report.firstBrokenLine)
                report.firstBrokenLine = lineNumber;
            report.intact = false;
            continue;
        }
        if (split->hash != chain::linkHash(previous, split->body) && !report.firstBrokenLine) {
            report.firstBrokenLine = lineNumber;
            report.intact = false;
        }
        report.bodies.push_back(split->body);
        report.headHash = split->hash;
        previous = split->hash;
    }
    return report;
}

} // namespace ovc::evidence
