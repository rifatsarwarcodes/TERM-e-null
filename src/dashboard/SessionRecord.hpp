#pragma once

#include "evidence/Journal.hpp"
#include "penalty/PenaltyEngine.hpp"
#include "session/Event.hpp"
#include "session/SessionStore.hpp"

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace ovc::dashboard {

// What the examiner needs to trust (or distrust) a session's record.
struct IntegrityReport {
    bool eventsFound = false;
    bool eventsIntact = true;
    std::optional<std::size_t> eventsBrokenAt;
    bool eventsTruncated = false;
    bool journalIntact = true;
    std::optional<std::size_t> journalBrokenAt;
    bool journalConsistent = true; // replay matched every erase
    bool ended = false;            // SESSION_END present
    bool sealPresent = false;
    bool sealMatchesLogs = false;  // seal.txt heads equal the logs' heads
    std::string receipt;           // computed from the logs
    std::optional<bool> submissionMatches; // submission file vs SESSION_END hash
    std::optional<bool> packageMatches;    // student's package vs examiner's copy
    std::optional<bool> pointsMatchLog;    // logged RULE_HIT points vs recalculated
    int resumes = 0;

    bool altered() const { return !eventsIntact || !journalIntact || !journalConsistent || (sealPresent && !sealMatchesLogs); }
    // "SEALED", "INTERRUPTED", "LOG ALTERED" or "NO LOG"
    std::string status() const;
};

struct PasteInfo {
    std::uint64_t sequence = 0;
    timeutil::Millis time = 0;
    std::string source;     // inside / self / allowed / outside
    std::string confidence; // confirmed / probable / certain
    long long chars = 0;
    long long lines = 0;
    std::string file;       // relative path of the saved text
    std::string note;
    double retained = 0.0;  // share still unchanged in the final code
    std::optional<timeutil::Millis> sinceFocusReturn;
};

// Share of the final code by origin, in characters.
struct OriginShares {
    std::size_t typed = 0, loaded = 0, inside = 0, self = 0, allowed = 0, outside = 0;
    std::size_t total() const { return typed + loaded + inside + self + allowed + outside; }
    double share(std::size_t part) const { return total() == 0 ? 0.0 : static_cast<double>(part) / total(); }
};

struct FocusItem {
    std::size_t firstLine = 0; // 1-based, inclusive
    std::size_t lastLine = 0;
    std::string reason;
    int priority = 0;          // higher first
};

// Everything loaded from one session folder and worked out from it.
struct SessionRecord {
    std::filesystem::path folder;
    session::SessionInfo info;
    std::vector<session::Event> events;
    std::vector<evidence::JournalEntry> journal;
    evidence::ReplayResult replay;
    IntegrityReport integrity;

    std::vector<PasteInfo> pastes;
    OriginShares shares;
    std::vector<penalty::RuleHit> hits; // recalculated
    std::map<std::string, int> pointsByRule;
    int totalPoints = 0;
    penalty::Level level = penalty::Level::Ok;
    penalty::PenaltyPolicy policy;
    std::vector<FocusItem> focus;

    int testsPassed = -1; // last TEST event, -1 if never tested
    int testsTotal = 0;
    timeutil::Millis timeAway = 0;
    timeutil::Millis duration = 0;

    const PasteInfo* paste(std::uint64_t sequence) const;
    std::string readFile(const std::string& relativePath) const;
};

} // namespace ovc::dashboard
