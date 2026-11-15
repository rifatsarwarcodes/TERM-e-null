#include "penalty/PenaltyEngine.hpp"

#include <cstdio>

namespace ovc::penalty {

using session::Event;
using session::EventType;

const char* levelName(Level level)
{
    switch (level) {
    case Level::Ok: return "OK";
    case Level::Warning: return "WARNING";
    case Level::Flagged: return "FLAGGED";
    }
    return "OK";
}

LevelStateMachine::LevelStateMachine(int warningAt, int flaggedAt)
    : warningAt_(warningAt)
    , flaggedAt_(flaggedAt)
{
}

bool LevelStateMachine::update(int totalPoints)
{
    Level next = level_;
    if (totalPoints >= flaggedAt_)
        next = Level::Flagged;
    else if (totalPoints >= warningAt_ && level_ == Level::Ok)
        next = Level::Warning;
    if (next == level_)
        return false;
    level_ = next;
    return true;
}

PenaltyEngine::PenaltyEngine(PenaltyPolicy policy, Publisher publish)
    : policy_(policy)
    , publish_(std::move(publish))
    , levels_(policy.warningAt, policy.flaggedAt)
{
    rules_.push_back(std::make_unique<LargePasteRule>(policy_));
    rules_.push_back(std::make_unique<RepeatedPasteRule>(policy_));
    rules_.push_back(std::make_unique<FocusReturnPasteRule>(policy_));
    rules_.push_back(std::make_unique<UnchangedPasteRule>(policy_));
    rules_.push_back(std::make_unique<PasscodeFailureRule>(policy_));
}

void PenaltyEngine::onEvent(const Event& event)
{
    // Our own output, and events logged by an earlier run, are not re-judged.
    if (event.type() == EventType::RuleHit || event.type() == EventType::LevelChange)
        return;

    for (auto& rule : rules_) {
        auto result = rule->evaluate(event);
        if (!result || result->points <= 0)
            continue;
        total_ += result->points;
        hits_.push_back(*result);
        if (publish_) {
            Event ruleHit(EventType::RuleHit);
            ruleHit.set("rule", result->rule)
                .set("points", static_cast<long long>(result->points))
                .set("event", static_cast<long long>(result->eventSequence))
                .set("detail", result->detail)
                .set("total", static_cast<long long>(total_));
            publish_(ruleHit);
        }
        if (levels_.update(total_) && publish_) {
            Event change(EventType::LevelChange);
            change.set("level", std::string(levelName(levels_.level()))).set("total", static_cast<long long>(total_));
            publish_(change);
        }
    }
}

std::map<std::string, int> PenaltyEngine::pointsByRule() const
{
    std::map<std::string, int> points;
    for (const auto& rule : rules_)
        points[rule->name()] = 0;
    for (const auto& hit : hits_)
        points[hit.rule] += hit.points;
    return points;
}

std::vector<std::string> PenaltyEngine::ruleNames() const
{
    std::vector<std::string> names;
    for (const auto& rule : rules_)
        names.emplace_back(rule->name());
    return names;
}

std::vector<std::string> describePolicy(const PenaltyPolicy& p)
{
    char ratio[8];
    std::snprintf(ratio, sizeof ratio, "%d%%", static_cast<int>(p.unchangedRatio * 100 + 0.5));
    return {
        "Copy-paste is allowed. Every paste is recorded with where it came from.",
        "Pasting your own code, the editor's own p/P, or allowed snippets costs nothing.",
        "Outside pastes up to " + std::to_string(p.smallPasteChars) + " characters and "
            + std::to_string(p.smallPasteLines) + " lines are only recorded.",
        "Large outside paste (over " + std::to_string(p.largePasteChars) + " characters or "
            + std::to_string(p.largePasteLines) + " lines): " + std::to_string(p.largePastePoints) + " points.",
        "More than " + std::to_string(p.repeatCount) + " outside pastes within "
            + std::to_string(p.repeatWindowMinutes) + " minutes: " + std::to_string(p.repeatPoints)
            + " points each.",
        "Outside paste within " + std::to_string(p.focusReturnSeconds)
            + " seconds of coming back to the editor window: " + std::to_string(p.focusReturnPoints) + " points.",
        "Outside paste still " + std::string(ratio) + " unchanged when you test or finish: "
            + std::to_string(p.unchangedPoints) + " points.",
        "Warning at " + std::to_string(p.warningAt) + " points, review by the examiner at "
            + std::to_string(p.flaggedAt) + " points.",
        "The program never takes away marks. The examiner decides after the viva.",
        "You can explain a paste: press Ctrl+N or type :note <text>.",
    };
}

} // namespace ovc::penalty
