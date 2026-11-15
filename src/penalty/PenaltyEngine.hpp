#pragma once

#include "penalty/PenaltyPolicy.hpp"
#include "penalty/PenaltyRule.hpp"
#include "session/EventBus.hpp"

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace ovc::penalty {

enum class Level { Ok, Warning, Flagged };

const char* levelName(Level level);

// OK -> Warning -> Flagged. The level only goes up during a session.
class LevelStateMachine {
public:
    LevelStateMachine(int warningAt, int flaggedAt);
    // Returns true if the level changed.
    bool update(int totalPoints);
    Level level() const { return level_; }

private:
    int warningAt_;
    int flaggedAt_;
    Level level_ = Level::Ok;
};

// Runs every rule on every event. During the exam it publishes RULE_HIT and
// LEVEL_CHANGE events through `publish`; in the dashboard the same engine is
// fed the saved events again to recalculate the points from the log itself.
// The tool never deducts marks: Flagged only asks the examiner to review.
class PenaltyEngine final : public session::IEventListener {
public:
    using Publisher = std::function<void(session::Event)>;

    explicit PenaltyEngine(PenaltyPolicy policy, Publisher publish = {});
    // Rules keep a reference to policy_, so the engine stays where it is.
    PenaltyEngine(const PenaltyEngine&) = delete;
    PenaltyEngine& operator=(const PenaltyEngine&) = delete;

    void onEvent(const session::Event& event) override;

    int total() const { return total_; }
    Level level() const { return levels_.level(); }
    const std::vector<RuleHit>& hits() const { return hits_; }
    std::map<std::string, int> pointsByRule() const;
    const PenaltyPolicy& policy() const { return policy_; }
    std::vector<std::string> ruleNames() const;

private:
    PenaltyPolicy policy_;
    std::vector<std::unique_ptr<PenaltyRule>> rules_;
    Publisher publish_;
    std::vector<RuleHit> hits_;
    int total_ = 0;
    LevelStateMachine levels_;
};

// The rules in plain words, for the student's :rules screen and reports.
std::vector<std::string> describePolicy(const PenaltyPolicy& policy);

} // namespace ovc::penalty
