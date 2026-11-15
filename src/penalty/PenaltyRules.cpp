#include "common/Strings.hpp"
#include "penalty/PenaltyRule.hpp"

#include <cstdio>

namespace ovc::penalty {

using session::Event;
using session::EventType;

namespace {

std::string pasteLabel(const Event& event)
{
    return "paste #" + std::to_string(event.sequence());
}

} // namespace

bool PenaltyRule::isCountedPaste(const Event& event) const
{
    if (event.type() != EventType::Paste || event.getOr("src") != "outside")
        return false;
    return event.getInt("chars") > policy_.smallPasteChars || event.getInt("lines") > policy_.smallPasteLines;
}

RuleHit PenaltyRule::hit(const Event& event, int points, std::string detail) const
{
    return RuleHit{name(), points, std::move(detail), event.sequence()};
}

std::optional<RuleHit> LargePasteRule::evaluate(const Event& event)
{
    if (!isCountedPaste(event))
        return std::nullopt;
    const long long chars = event.getInt("chars");
    const long long lines = event.getInt("lines");
    if (chars <= policy_.largePasteChars && lines <= policy_.largePasteLines)
        return std::nullopt;
    return hit(event, policy_.largePastePoints,
        pasteLabel(event) + ": " + std::to_string(chars) + " chars, " + std::to_string(lines) + " lines");
}

std::optional<RuleHit> RepeatedPasteRule::evaluate(const Event& event)
{
    if (!isCountedPaste(event))
        return std::nullopt;
    const timeutil::Millis window = static_cast<timeutil::Millis>(policy_.repeatWindowMinutes) * 60'000;
    while (!recent_.empty() && event.time() - recent_.front() > window)
        recent_.pop_front();
    recent_.push_back(event.time());
    if (static_cast<int>(recent_.size()) <= policy_.repeatCount)
        return std::nullopt;
    return hit(event, policy_.repeatPoints,
        pasteLabel(event) + ": " + std::to_string(recent_.size()) + " outside pastes within "
            + std::to_string(policy_.repeatWindowMinutes) + " min");
}

std::optional<RuleHit> FocusReturnPasteRule::evaluate(const Event& event)
{
    if (event.type() == EventType::FocusGained) {
        if (event.getInt("away_ms") >= MinimumAway)
            lastReturn_ = event.time();
        return std::nullopt;
    }
    if (!isCountedPaste(event) || !lastReturn_)
        return std::nullopt;
    const timeutil::Millis since = event.time() - *lastReturn_;
    if (since > static_cast<timeutil::Millis>(policy_.focusReturnSeconds) * 1000)
        return std::nullopt;
    return hit(event, policy_.focusReturnPoints,
        pasteLabel(event) + ": " + std::to_string(since / 1000) + " s after returning to the editor");
}

std::optional<RuleHit> UnchangedPasteRule::evaluate(const Event& event)
{
    if (isCountedPaste(event)) {
        counted_.insert(event.sequence());
        return std::nullopt;
    }
    if (event.type() != EventType::Test && event.type() != EventType::SessionEnd)
        return std::nullopt;

    // "retained" lists pastes still in the file: "24:0.93,31:0.40".
    int points = 0;
    std::string detail;
    for (const auto& item : strings::split(event.getOr("retained"), ',')) {
        const auto parts = strings::split(item, ':');
        std::uint64_t sequence = 0;
        double ratio = 0;
        if (parts.size() != 2 || !strings::parseUnsigned(parts[0], sequence) || !strings::parseDouble(parts[1], ratio))
            continue;
        if (counted_.count(sequence) == 0 || penalised_.count(sequence) != 0 || ratio < policy_.unchangedRatio)
            continue;
        penalised_.insert(sequence);
        points += policy_.unchangedPoints;
        char percent[16];
        std::snprintf(percent, sizeof percent, "%d%%", static_cast<int>(ratio * 100 + 0.5));
        detail += (detail.empty() ? "" : ", ") + std::string("paste #") + std::to_string(sequence) + " " + percent
            + " unchanged";
    }
    if (points == 0)
        return std::nullopt;
    return hit(event, points, detail);
}

std::optional<RuleHit> PasscodeFailureRule::evaluate(const Event& event)
{
    if (fired_ || event.type() != EventType::PasscodeFailed)
        return std::nullopt;
    if (event.getInt("attempts") < policy_.passcodeFailLimit)
        return std::nullopt;
    fired_ = true;
    return hit(event, policy_.flaggedAt, std::to_string(event.getInt("attempts")) + " wrong passcode attempts");
}

} // namespace ovc::penalty
