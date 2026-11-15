#pragma once

#include "common/Time.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace ovc::session {

namespace fs = std::filesystem;

// Facts about a session that do not change while it runs.
struct SessionInfo {
    std::string studentName;
    std::string studentId;
    std::string examCode;
    std::string examTitle;
    std::string fileName;          // the file being edited, e.g. main.cpp
    timeutil::Millis startedAt = 0;
    timeutil::Millis timeLimit = 0; // 0 means no limit
    std::string packageHash;       // SHA-256 of the exam package used

    std::string encode() const;
    static std::optional<SessionInfo> decode(const std::string& text);
};

enum class SessionState {
    New,         // no folder yet
    Interrupted, // folder exists but the session was never sealed
    Sealed,      // ended normally or by the time limit
};

// Student IDs become folder names, so only simple characters are allowed.
bool isValidStudentId(const std::string& id);

// Where one student's session lives on disk:
//
//   <root>/<student id>/
//       session.info     events.log     journal.log     seal.txt
//       pastes/NNN.txt   runs/NNN.txt   submission/<file>   build/
class SessionStore {
public:
    SessionStore(fs::path root, std::string studentId);
    explicit SessionStore(fs::path folder);

    const fs::path& folder() const { return folder_; }
    fs::path infoFile() const { return folder_ / "session.info"; }
    fs::path eventsLog() const { return folder_ / "events.log"; }
    fs::path journalLog() const { return folder_ / "journal.log"; }
    fs::path sealFile() const { return folder_ / "seal.txt"; }
    fs::path pastesDir() const { return folder_ / "pastes"; }
    fs::path runsDir() const { return folder_ / "runs"; }
    fs::path submissionDir() const { return folder_ / "submission"; }
    fs::path buildDir() const { return folder_ / "build"; }
    fs::path pasteFile(std::uint64_t sequence) const;
    fs::path runFile(std::uint64_t sequence) const;

    SessionState state() const;
    bool create() const;

    bool writeInfo(const SessionInfo& info) const;
    std::optional<SessionInfo> readInfo() const;

private:
    fs::path folder_;
};

} // namespace ovc::session
