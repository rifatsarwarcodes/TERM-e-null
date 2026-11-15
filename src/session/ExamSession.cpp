#include "session/ExamSession.hpp"

#include <algorithm>
#include <limits>

namespace ovc::session {

SessionClock::SessionClock(timeutil::Millis startedAt, timeutil::Millis timeLimit, NowFunction wallNow,
    NowFunction steadyNow)
    : timeLimit_(timeLimit)
    , elapsedAtLaunch_(std::max<timeutil::Millis>(0, wallNow() - startedAt))
    , steadyAtLaunch_(steadyNow())
    , wallNow_(std::move(wallNow))
    , steadyNow_(std::move(steadyNow))
{
}

timeutil::Millis SessionClock::elapsed() const
{
    return elapsedAtLaunch_ + (steadyNow_() - steadyAtLaunch_);
}

timeutil::Millis SessionClock::remaining() const
{
    if (!hasLimit())
        return std::numeric_limits<timeutil::Millis>::max() / 2;
    return timeLimit_ - elapsed();
}

ExamSession::ExamSession(SessionInfo info, SessionStore store, EventBus& bus, std::uint64_t lastSequence,
    SessionClock clock)
    : info_(std::move(info))
    , store_(std::move(store))
    , bus_(bus)
    , lastSequence_(lastSequence)
    , clock_(std::move(clock))
{
}

Event ExamSession::publish(Event event)
{
    event.stamp(++lastSequence_, clock_.now());
    bus_.publish(event);
    return event;
}

} // namespace ovc::session
