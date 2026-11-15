#include "TestFramework.hpp"

#include "common/Escape.hpp"
#include "session/EventBus.hpp"
#include "session/ExamSession.hpp"
#include "session/SessionStore.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace ovc;
using namespace ovc::session;

TEST_CASE("escape: round trip of separators and newlines")
{
    const std::string text = "a|b;c=d\\e\nf";
    CHECK_EQ(escape::decode(escape::encode(text)), text);
    CHECK(escape::encode(text).find('|') == std::string::npos);
    CHECK_EQ(escape::findUnescaped("a\\|b|c", '|'), std::size_t(4));
}

TEST_CASE("event: encode and decode keep every field")
{
    Event event(EventType::Paste);
    event.set("src", std::string("outside")).set("chars", 1240LL).set("note", std::string("x=1; y|2"));
    event.stamp(24, 1700000000123);
    const std::string line = event.encode();
    CHECK(line.rfind("24|1700000000123|PASTE|", 0) == 0);

    const auto decoded = Event::decode(line);
    CHECK(decoded.has_value());
    CHECK(decoded->type() == EventType::Paste);
    CHECK_EQ(decoded->sequence(), std::uint64_t(24));
    CHECK_EQ(decoded->getInt("chars"), 1240LL);
    CHECK_EQ(decoded->getOr("note"), std::string("x=1; y|2"));
}

TEST_CASE("event: broken lines are rejected")
{
    CHECK(!Event::decode("not a log line").has_value());
    CHECK(!Event::decode("x|1|SAVE|").has_value());
    CHECK(Event::decode("3|1|SAVE|").has_value());
}

namespace {

struct Collector : IEventListener {
    std::vector<std::string> seen;
    void onEvent(const Event& event) override { seen.push_back(eventTypeName(event.type())); }
};

struct Reactor : IEventListener {
    EventBus* bus = nullptr;
    void onEvent(const Event& event) override
    {
        if (event.type() == EventType::Paste)
            bus->publish(Event(EventType::RuleHit));
    }
};

} // namespace

TEST_CASE("event bus: nested publishes are delivered in order to everyone")
{
    EventBus bus;
    Reactor reactor;
    reactor.bus = &bus;
    Collector first;
    Collector last;
    bus.subscribe(&first);
    bus.subscribe(&reactor);
    bus.subscribe(&last);
    bus.publish(Event(EventType::Paste));
    CHECK_EQ(first.seen.size(), std::size_t(2));
    CHECK_EQ(last.seen.size(), std::size_t(2));
    CHECK_EQ(last.seen[0], std::string("PASTE"));
    CHECK_EQ(last.seen[1], std::string("RULE_HIT"));
}

TEST_CASE("session clock: monotonic time decides the remaining time")
{
    timeutil::Millis wall = 10'000;
    timeutil::Millis steady = 500;
    SessionClock clock(4'000, 60'000, [&] { return wall; }, [&] { return steady; });
    CHECK_EQ(clock.elapsed(), timeutil::Millis(6'000));
    steady += 1'000;
    wall -= 3'600'000; // someone sets the system clock back an hour
    CHECK_EQ(clock.remaining(), timeutil::Millis(53'000));
    steady += 60'000;
    CHECK(clock.timeUp());
}

TEST_CASE("session: publish stamps increasing sequence numbers")
{
    EventBus bus;
    Collector collector;
    bus.subscribe(&collector);
    SessionInfo info;
    info.studentId = "240042125";
    ExamSession session(info, SessionStore("sessions", "240042125"), bus, 4, SessionClock(0, 0));
    const Event saved = session.publish(Event(EventType::Save));
    CHECK_EQ(saved.sequence(), std::uint64_t(5));
    CHECK_EQ(session.publish(Event(EventType::Save)).sequence(), std::uint64_t(6));
    CHECK_EQ(collector.seen.size(), std::size_t(2));
}

TEST_CASE("session store: state follows the files on disk")
{
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / "ovc_test_store";
    fs::remove_all(root);
    SessionStore store(root, "240042125");
    CHECK(store.state() == SessionState::New);
    CHECK(store.create());

    SessionInfo info;
    info.studentName = "Student A";
    info.studentId = "240042125";
    info.timeLimit = 90 * 60 * 1000;
    CHECK(store.writeInfo(info));
    CHECK(store.state() == SessionState::Interrupted);
    const auto read = store.readInfo();
    CHECK(read && read->studentName == "Student A" && read->timeLimit == info.timeLimit);

    std::ofstream(store.sealFile()) << "sealed\n";
    CHECK(store.state() == SessionState::Sealed);
    CHECK_EQ(store.pasteFile(24).filename().string(), std::string("024.txt"));
    fs::remove_all(root);
}

TEST_CASE("session store: student ids must be safe folder names")
{
    CHECK(isValidStudentId("240042125"));
    CHECK(isValidStudentId("STU-001"));
    CHECK(!isValidStudentId("../etc"));
    CHECK(!isValidStudentId(""));
    CHECK(!isValidStudentId("a b"));
}
