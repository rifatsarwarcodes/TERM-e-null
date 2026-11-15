#include "TestFramework.hpp"

#include "platform/InputDecoder.hpp"

#include <variant>

using namespace ovc::platform;

namespace {

std::vector<InputEvent> decode(const std::string& bytes, bool flush = true)
{
    InputDecoder decoder;
    std::vector<InputEvent> events;
    decoder.feed(bytes, events);
    if (flush)
        decoder.flushPending(events);
    return events;
}

const KeyEvent* keyAt(const std::vector<InputEvent>& events, std::size_t index)
{
    return index < events.size() ? std::get_if<KeyEvent>(&events[index]) : nullptr;
}

} // namespace

TEST_CASE("input: plain characters and UTF-8")
{
    const auto events = decode("a─");
    CHECK_EQ(events.size(), std::size_t(2));
    CHECK(keyAt(events, 0) && keyAt(events, 0)->isChar('a'));
    CHECK(keyAt(events, 1) && keyAt(events, 1)->text == "─");
}

TEST_CASE("input: control keys")
{
    const auto events = decode("\x13\r\x7f\t");
    CHECK(keyAt(events, 0) && keyAt(events, 0)->isCtrl('s'));
    CHECK(keyAt(events, 1) && keyAt(events, 1)->is(KeyCode::Enter));
    CHECK(keyAt(events, 2) && keyAt(events, 2)->is(KeyCode::Backspace));
    CHECK(keyAt(events, 3) && keyAt(events, 3)->is(KeyCode::Tab));
}

TEST_CASE("input: arrows, function keys and modifiers")
{
    const auto events = decode("\x1b[A\x1bOQ\x1b[3~\x1b[15~\x1b[1;5C");
    CHECK_EQ(events.size(), std::size_t(5));
    CHECK(keyAt(events, 0) && keyAt(events, 0)->is(KeyCode::Up));
    CHECK(keyAt(events, 1) && keyAt(events, 1)->is(KeyCode::F2));
    CHECK(keyAt(events, 2) && keyAt(events, 2)->is(KeyCode::Delete));
    CHECK(keyAt(events, 3) && keyAt(events, 3)->is(KeyCode::F5));
    CHECK(keyAt(events, 4) && keyAt(events, 4)->code == KeyCode::Right && keyAt(events, 4)->ctrl);
}

TEST_CASE("input: lone escape needs a flush")
{
    InputDecoder decoder;
    std::vector<InputEvent> events;
    decoder.feed("\x1b", events);
    CHECK(events.empty());
    CHECK(decoder.hasPending());
    decoder.flushPending(events);
    CHECK(keyAt(events, 0) && keyAt(events, 0)->is(KeyCode::Escape));
}

TEST_CASE("input: sequences split across reads")
{
    InputDecoder decoder;
    std::vector<InputEvent> events;
    decoder.feed("\x1b[", events);
    decoder.feed("1;2", events);
    CHECK(events.empty());
    decoder.feed("B", events);
    CHECK(keyAt(events, 0) && keyAt(events, 0)->code == KeyCode::Down && keyAt(events, 0)->shift);
}

TEST_CASE("input: bracketed paste becomes one paste event")
{
    const auto events = decode("x\x1b[200~int a;\r\nint b;\x1b[201~y");
    CHECK_EQ(events.size(), std::size_t(3));
    const auto* paste = std::get_if<PasteEvent>(&events[1]);
    CHECK(paste != nullptr);
    CHECK(paste && paste->text == "int a;\nint b;");
    CHECK(keyAt(events, 2) && keyAt(events, 2)->isChar('y'));
}

TEST_CASE("input: paste end marker split across reads")
{
    InputDecoder decoder;
    std::vector<InputEvent> events;
    decoder.feed("\x1b[200~hello\x1b[20", events);
    CHECK(events.empty());
    CHECK(decoder.inPaste());
    decoder.feed("1~", events);
    CHECK_EQ(events.size(), std::size_t(1));
    const auto* paste = std::get_if<PasteEvent>(&events[0]);
    CHECK(paste && paste->text == "hello");
}

TEST_CASE("input: focus reports")
{
    const auto events = decode("\x1b[O\x1b[I");
    CHECK_EQ(events.size(), std::size_t(2));
    const auto* lost = std::get_if<FocusEvent>(&events[0]);
    const auto* gained = std::get_if<FocusEvent>(&events[1]);
    CHECK(lost && !lost->gained);
    CHECK(gained && gained->gained);
}

TEST_CASE("input: alt + character")
{
    const auto events = decode("\x1bx");
    CHECK(keyAt(events, 0) && keyAt(events, 0)->alt && keyAt(events, 0)->text == "x");
}
