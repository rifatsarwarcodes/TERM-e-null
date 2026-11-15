#include "TestFramework.hpp"

#include "penalty/PenaltyEngine.hpp"

#include <vector>

using namespace ovc;
using namespace ovc::penalty;
using session::Event;
using session::EventType;

namespace {

std::uint64_t nextSequence = 1;

Event at(Event event, timeutil::Millis seconds)
{
    event.stamp(nextSequence++, seconds * 1000);
    return event;
}

Event paste(const std::string& source, long long chars, long long lines, timeutil::Millis seconds)
{
    Event event(EventType::Paste);
    event.set("src", source).set("chars", chars).set("lines", lines);
    return at(event, seconds);
}

} // namespace

TEST_CASE("penalty: small and self pastes cost nothing")
{
    PenaltyEngine engine(PenaltyPolicy{});
    engine.onEvent(paste("outside", 40, 1, 10));
    engine.onEvent(paste("self", 2000, 60, 20));
    engine.onEvent(paste("allowed", 500, 12, 30));
    CHECK_EQ(engine.total(), 0);
    CHECK(engine.level() == Level::Ok);
}

TEST_CASE("penalty: large outside paste reaches warning")
{
    std::vector<Event> published;
    PenaltyEngine engine(PenaltyPolicy{}, [&](Event e) { published.push_back(e); });
    engine.onEvent(paste("outside", 1240, 46, 100));
    CHECK_EQ(engine.total(), 3);
    CHECK(engine.level() == Level::Warning);
    CHECK_EQ(published.size(), std::size_t(2));
    CHECK(published[0].type() == EventType::RuleHit);
    CHECK(published[1].type() == EventType::LevelChange);
    CHECK_EQ(published[1].getOr("level"), std::string("WARNING"));
}

TEST_CASE("penalty: repeated pastes inside the window")
{
    PenaltyEngine engine(PenaltyPolicy{});
    for (int i = 0; i < 4; ++i)
        engine.onEvent(paste("outside", 120, 3, i * 60));
    CHECK_EQ(engine.pointsByRule()["Repeated pastes"], 2);
    // A paste long after the window does not count as repeated.
    engine.onEvent(paste("outside", 120, 3, 4 * 3600));
    CHECK_EQ(engine.pointsByRule()["Repeated pastes"], 2);
}

TEST_CASE("penalty: paste soon after coming back to the window")
{
    PenaltyEngine engine(PenaltyPolicy{});
    Event back(EventType::FocusGained);
    back.set("away_ms", 220'000LL);
    engine.onEvent(at(back, 1000));
    engine.onEvent(paste("outside", 150, 5, 1006));
    CHECK_EQ(engine.pointsByRule()["Paste after coming back"], 2);

    // A short glance away does not count.
    PenaltyEngine other(PenaltyPolicy{});
    Event glance(EventType::FocusGained);
    glance.set("away_ms", 2'000LL);
    other.onEvent(at(glance, 1000));
    other.onEvent(paste("outside", 150, 5, 1005));
    CHECK_EQ(other.total(), 0);
}

TEST_CASE("penalty: unchanged paste is counted once when tested")
{
    PenaltyEngine engine(PenaltyPolicy{});
    const Event pasted = paste("outside", 150, 5, 10);
    engine.onEvent(pasted);
    const std::string id = std::to_string(pasted.sequence());

    Event test(EventType::Test);
    test.set("retained", id + ":0.93");
    engine.onEvent(at(test, 20));
    CHECK_EQ(engine.pointsByRule()["Paste left unchanged"], 3);

    Event end(EventType::SessionEnd);
    end.set("retained", id + ":0.95");
    engine.onEvent(at(end, 30));
    CHECK_EQ(engine.pointsByRule()["Paste left unchanged"], 3);

    // An edited paste is not penalised.
    PenaltyEngine edited(PenaltyPolicy{});
    const Event other = paste("outside", 150, 5, 10);
    edited.onEvent(other);
    Event laterTest(EventType::Test);
    laterTest.set("retained", std::to_string(other.sequence()) + ":0.40");
    edited.onEvent(at(laterTest, 20));
    CHECK_EQ(edited.total(), 0);
}

TEST_CASE("penalty: three wrong passcodes flag the session")
{
    PenaltyEngine engine(PenaltyPolicy{});
    for (int attempt = 1; attempt <= 3; ++attempt) {
        Event failed(EventType::PasscodeFailed);
        failed.set("attempts", static_cast<long long>(attempt));
        engine.onEvent(at(failed, attempt));
    }
    CHECK(engine.level() == Level::Flagged);
}

TEST_CASE("penalty: level never goes back down and logged hits are ignored")
{
    LevelStateMachine levels(3, 8);
    CHECK(levels.update(9));
    CHECK(levels.level() == Level::Flagged);
    CHECK(!levels.update(0));
    CHECK(levels.level() == Level::Flagged);

    PenaltyEngine engine(PenaltyPolicy{});
    Event oldHit(EventType::RuleHit);
    oldHit.set("points", 50LL);
    engine.onEvent(at(oldHit, 1));
    CHECK_EQ(engine.total(), 0);
}

TEST_CASE("penalty: policy is described in plain words")
{
    const auto lines = describePolicy(PenaltyPolicy{});
    CHECK(lines.size() > 5);
    CHECK(lines[3].find("300") != std::string::npos);
}
