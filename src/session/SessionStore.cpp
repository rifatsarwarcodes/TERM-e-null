#include "session/SessionStore.hpp"

#include "common/Escape.hpp"
#include "common/Files.hpp"
#include "common/Strings.hpp"

#include <cctype>
#include <cstdio>

namespace ovc::session {

namespace {

std::string numbered(std::uint64_t sequence)
{
    char name[32];
    std::snprintf(name, sizeof name, "%03llu.txt", static_cast<unsigned long long>(sequence));
    return name;
}

} // namespace

std::string SessionInfo::encode() const
{
    std::string text;
    auto line = [&text](const char* key, const std::string& value) {
        text += std::string(key) + "=" + escape::encode(value) + "\n";
    };
    line("student_name", studentName);
    line("student_id", studentId);
    line("exam_code", examCode);
    line("exam_title", examTitle);
    line("file", fileName);
    line("started_at", std::to_string(startedAt));
    line("time_limit_ms", std::to_string(timeLimit));
    line("package_hash", packageHash);
    return text;
}

std::optional<SessionInfo> SessionInfo::decode(const std::string& text)
{
    SessionInfo info;
    bool sawId = false;
    for (const auto& raw : strings::splitLines(text)) {
        const std::size_t equals = raw.find('=');
        if (equals == std::string::npos)
            continue;
        const std::string key = raw.substr(0, equals);
        const std::string value = escape::decode(raw.substr(equals + 1));
        long long number = 0;
        if (key == "student_name") info.studentName = value;
        else if (key == "student_id") { info.studentId = value; sawId = true; }
        else if (key == "exam_code") info.examCode = value;
        else if (key == "exam_title") info.examTitle = value;
        else if (key == "file") info.fileName = value;
        else if (key == "started_at" && strings::parseInt(value, number)) info.startedAt = number;
        else if (key == "time_limit_ms" && strings::parseInt(value, number)) info.timeLimit = number;
        else if (key == "package_hash") info.packageHash = value;
    }
    if (!sawId)
        return std::nullopt;
    return info;
}

bool isValidStudentId(const std::string& id)
{
    if (id.empty() || id.size() > 32)
        return false;
    for (char c : id)
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '-' && c != '_')
            return false;
    return true;
}

SessionStore::SessionStore(fs::path root, std::string studentId)
    : folder_(std::move(root) / studentId)
{
}

SessionStore::SessionStore(fs::path folder)
    : folder_(std::move(folder))
{
}

fs::path SessionStore::pasteFile(std::uint64_t sequence) const
{
    return pastesDir() / numbered(sequence);
}

fs::path SessionStore::runFile(std::uint64_t sequence) const
{
    return runsDir() / numbered(sequence);
}

SessionState SessionStore::state() const
{
    std::error_code error;
    if (!fs::exists(infoFile(), error))
        return SessionState::New;
    if (fs::exists(sealFile(), error))
        return SessionState::Sealed;
    return SessionState::Interrupted;
}

bool SessionStore::create() const
{
    return files::ensureDirectory(folder_) && files::ensureDirectory(pastesDir())
        && files::ensureDirectory(runsDir()) && files::ensureDirectory(submissionDir())
        && files::ensureDirectory(buildDir());
}

bool SessionStore::writeInfo(const SessionInfo& info) const
{
    return files::writeAtomically(infoFile(), info.encode());
}

std::optional<SessionInfo> SessionStore::readInfo() const
{
    const auto text = files::read(infoFile());
    if (!text)
        return std::nullopt;
    return SessionInfo::decode(*text);
}

} // namespace ovc::session
