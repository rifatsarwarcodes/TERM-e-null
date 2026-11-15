#pragma once

#include "penalty/PenaltyPolicy.hpp"
#include "session/Event.hpp"

#include <cstdint>
#include <deque>
#include <optional>
#include <set>
#include <string>

namespace ovc::penalty {

struct RuleHit {
    std::string rule;
    int points = 0;
    std::string detail;
    std::uint64_t eventSequence = 0; // the event that triggered the rule
};

// One rule of the paste policy. Rules see every event in order and keep
// whatever state they need (time windows, pastes already counted), so each
// event is evaluated in constant time.
class PenaltyRule {
public:
    explicit PenaltyRule(const PenaltyPolicy& policy)
        : policy_(policy)
    {
    }
    virtual ~PenaltyRule() = default;

    virtual const char* name() const = 0;
    virtual std::optional<RuleHit> evaluate(const session::Event& event) = 0;

protected:
    // Outside pastes above the "small" limit are the ones that can cost points.
    bool isCountedPaste(const session::Event& event) const;
    RuleHit hit(const session::Event& event, int points, std::string detail) const;

    const PenaltyPolicy& policy_;
};

class LargePasteRule final : public PenaltyRule {
public:
    using PenaltyRule::PenaltyRule;
    const char* name() const override { return "Large outside paste"; }
    std::optional<RuleHit> evaluate(const session::Event& event) override;
};

class RepeatedPasteRule final : public PenaltyRule {
public:
    using PenaltyRule::PenaltyRule;
    const char* name() const override { return "Repeated pastes"; }
    std::optional<RuleHit> evaluate(const session::Event& event) override;

private:
    std::deque<timeutil::Millis> recent_;
};

class FocusReturnPasteRule final : public PenaltyRule {
public:
    using PenaltyRule::PenaltyRule;
    const char* name() const override { return "Paste after coming back"; }
    std::optional<RuleHit> evaluate(const session::Event& event) override;

    // Only returns after being away at least this long count.
    static constexpr timeutil::Millis MinimumAway = 10'000;

private:
    std::optional<timeutil::Millis> lastReturn_;
};

class UnchangedPasteRule final : public PenaltyRule {
public:
    using PenaltyRule::PenaltyRule;
    const char* name() const override { return "Paste left unchanged"; }
    std::optional<RuleHit> evaluate(const session::Event& event) override;

private:
    std::set<std::uint64_t> counted_; // outside pastes worth checking
    std::set<std::uint64_t> penalised_;
};

class PasscodeFailureRule final : public PenaltyRule {
public:
    using PenaltyRule::PenaltyRule;
    const char* name() const override { return "Wrong examiner passcode"; }
    std::optional<RuleHit> evaluate(const session::Event& event) override;

private:
    bool fired_ = false;
};

} // namespace ovc::penalty
