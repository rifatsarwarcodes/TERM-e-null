#pragma once

#include "common/Time.hpp"
#include "session/EventBus.hpp"
#include "session/SessionStore.hpp"

#include <cstdint>
#include <functional>

namespace ovc::session {

// Time inside a session. Elapsed time is counted with the monotonic clock
// from the moment this run started, plus the wall-clock time that had
// already passed since the original start (after a resume). Changing the
// system clock while the program runs does not change the remaining time.
class SessionClock {
public:
    using NowFunction = std::function<timeutil::Millis()>;

    SessionClock(timeutil::Millis startedAt, timeutil::Millis timeLimit,
        NowFunction wallNow = timeutil::nowEpochMillis, NowFunction steadyNow = timeutil::nowSteadyMillis);

    timeutil::Millis elapsed() const;
    timeutil::Millis remaining() const; // large when there is no limit
    bool hasLimit() const { return timeLimit_ > 0; }
    bool timeUp() const { return hasLimit() && remaining() <= 0; }
    timeutil::Millis timeLimit() const { return timeLimit_; }
    timeutil::Millis now() const { return wallNow_(); }

private:
    timeutil::Millis timeLimit_;
    timeutil::Millis elapsedAtLaunch_;
    timeutil::Millis steadyAtLaunch_;
    NowFunction wallNow_;
    NowFunction steadyNow_;
};

// One student's sitting: who, which exam, the clock, and the numbering of
// events. Every event goes through publish(), which stamps it with the next
// sequence number and the time, then hands it to the event bus.
class ExamSession {
public:
    ExamSession(SessionInfo info, SessionStore store, EventBus& bus, std::uint64_t lastSequence,
        SessionClock clock);

    const SessionInfo& info() const { return info_; }
    const SessionStore& store() const { return store_; }
    const SessionClock& clock() const { return clock_; }
    EventBus& bus() { return bus_; }

    // Stamps and publishes; returns the stamped copy.
    Event publish(Event event);
    std::uint64_t lastSequence() const { return lastSequence_; }
    std::uint64_t nextSequence() const { return lastSequence_ + 1; }

private:
    SessionInfo info_;
    SessionStore store_;
    EventBus& bus_;
    std::uint64_t lastSequence_;
    SessionClock clock_;
};

} // namespace ovc::session
