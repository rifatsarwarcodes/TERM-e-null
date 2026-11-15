#include "dashboard/ReportWriter.hpp"

#include "common/Time.hpp"

#include <cstdio>

namespace ovc::dashboard {

using session::EventType;

std::string percentText(double ratio)
{
    char text[8];
    std::snprintf(text, sizeof text, "%d%%", static_cast<int>(ratio * 100 + 0.5));
    return text;
}

namespace {

std::string yesNo(const std::optional<bool>& value, const char* yes, const char* no)
{
    if (!value)
        return "not checked";
    return *value ? yes : no;
}

std::string padded(const std::string& text, std::size_t width)
{
    return text.size() >= width ? text.substr(0, width) : text + std::string(width - text.size(), ' ');
}

} // namespace

std::string ReportWriter::sessionReport(const SessionRecord& record)
{
    const IntegrityReport& integrity = record.integrity;
    std::string out;
    out += "OV_C session report\n===================\n\n";
    out += "Student:   " + record.info.studentName + " (" + record.info.studentId + ")\n";
    out += "Exam:      " + record.info.examCode + "  " + record.info.examTitle + "\n";
    out += "File:      " + record.info.fileName + "\n";
    out += "Started:   " + timeutil::formatDateTime(record.info.startedAt) + "\n";
    out += "Duration:  " + timeutil::formatSpan(record.duration) + "\n\n";

    out += "Integrity\n---------\n";
    out += "Status:            " + integrity.status() + "\n";
    out += "Event log chain:   " + std::string(integrity.eventsIntact ? "intact" : "BROKEN at line " + std::to_string(integrity.eventsBrokenAt.value_or(0))) + "\n";
    out += "Edit journal:      " + std::string(integrity.journalIntact && integrity.journalConsistent ? "intact" : "BROKEN or inconsistent") + "\n";
    out += "Seal:              " + std::string(!integrity.sealPresent ? "missing" : integrity.sealMatchesLogs ? "matches the logs" : "DOES NOT MATCH the logs") + "\n";
    out += "Receipt code:      " + integrity.receipt + "  (compare with the attendance sheet)\n";
    out += "Submitted file:    " + yesNo(integrity.submissionMatches, "matches SESSION_END", "DIFFERENT from SESSION_END") + "\n";
    out += "Exam package:      " + yesNo(integrity.packageMatches, "same as the examiner's copy", "DIFFERENT from the examiner's copy") + "\n";
    out += "Resumed:           " + std::to_string(integrity.resumes) + " time(s)\n\n";

    out += "Code origin (final file)\n------------------------\n";
    const OriginShares& s = record.shares;
    out += "Typed " + percentText(s.share(s.typed)) + ", outside paste " + percentText(s.share(s.outside))
        + ", self/inside paste " + percentText(s.share(s.self + s.inside)) + ", allowed/loaded "
        + percentText(s.share(s.allowed + s.loaded)) + "\n\n";

    out += "Pastes\n------\n";
    if (record.pastes.empty())
        out += "None.\n";
    for (const auto& paste : record.pastes) {
        out += "#" + std::to_string(paste.sequence) + "  " + timeutil::formatClock(paste.time) + "  " + paste.source
            + " (" + paste.confidence + "), " + std::to_string(paste.chars) + " chars, " + std::to_string(paste.lines)
            + " lines, " + percentText(paste.retained) + " kept";
        if (!paste.note.empty())
            out += "\n      note: \"" + paste.note + "\"";
        out += "\n";
    }

    out += "\nCompile, run and test history\n-----------------------------\n";
    for (const auto& event : record.events) {
        const std::string when = timeutil::formatClock(event.time());
        if (event.type() == EventType::Compile)
            out += when + "  COMPILE  " + (event.getInt("ok") ? "ok" : std::to_string(event.getInt("errors")) + " error(s)") + "\n";
        else if (event.type() == EventType::Run)
            out += when + "  RUN      " + event.getOr("mode") + ", exit " + std::to_string(event.getInt("exit")) + "\n";
        else if (event.type() == EventType::Test)
            out += when + "  TEST     " + std::to_string(event.getInt("passed")) + "/" + std::to_string(event.getInt("total")) + " passed\n";
    }

    out += "\nPenalty (recalculated with the examiner's rules)\n------------------------------------------------\n";
    for (const auto& [rule, points] : record.pointsByRule)
        out += padded(rule, 30) + std::to_string(points) + "\n";
    out += padded("Total", 30) + std::to_string(record.totalPoints) + "\n";
    out += "Level: " + std::string(penalty::levelName(record.level)) + "\n";
    if (record.integrity.pointsMatchLog && !*record.integrity.pointsMatchLog)
        out += "Note: the points written in the log differ from the recalculated points.\n";

    out += "\nViva focus list\n---------------\n";
    if (record.focus.empty())
        out += "Nothing in particular.\n";
    for (std::size_t i = 0; i < record.focus.size(); ++i) {
        const FocusItem& item = record.focus[i];
        out += std::to_string(i + 1) + ". Lines " + std::to_string(item.firstLine) + "-" + std::to_string(item.lastLine)
            + ": " + item.reason + "\n";
    }
    out += "\nThe tool does not deduct marks. Any decision is made by the examiner.\n";
    return out;
}

std::string ReportWriter::classSummary(const std::vector<SessionRecord>& records)
{
    std::string out = "OV_C class summary\n==================\n\n";
    out += padded("ID", 12) + padded("Name", 22) + padded("Integrity", 14) + padded("Mins", 6) + padded("Typed", 7)
        + padded("Outside", 9) + padded("Tests", 7) + padded("Away", 9) + "Level\n";
    for (const auto& record : records) {
        const OriginShares& s = record.shares;
        const std::string tests = record.testsPassed < 0 ? "-" : std::to_string(record.testsPassed) + "/" + std::to_string(record.testsTotal);
        out += padded(record.info.studentId, 12) + padded(record.info.studentName, 22)
            + padded(record.integrity.status(), 14) + padded(std::to_string(record.duration / 60000), 6)
            + padded(percentText(s.share(s.typed)), 7) + padded(percentText(s.share(s.outside)), 9) + padded(tests, 7)
            + padded(timeutil::formatSpan(record.timeAway), 9) + penalty::levelName(record.level) + "\n";
    }
    return out;
}

} // namespace ovc::dashboard
