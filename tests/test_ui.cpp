#include "TestFramework.hpp"

#include "ui/AnsiRenderer.hpp"
#include "ui/ScreenBuffer.hpp"
#include "ui/Theme.hpp"

using namespace ovc::ui;

TEST_CASE("screen: text is clipped to the buffer")
{
    ScreenBuffer screen(5, 2);
    const int used = screen.text(3, 0, "hello", theme::text());
    CHECK_EQ(used, 5);
    CHECK_EQ(screen.at(4, 0).glyph, std::string("e"));
    screen.text(-1, 1, "ab", theme::text());
    CHECK_EQ(screen.at(0, 1).glyph, std::string("b"));
}

TEST_CASE("screen: field pads and truncates")
{
    ScreenBuffer screen(10, 1);
    screen.field(0, 0, 4, "ab", theme::text());
    CHECK_EQ(screen.at(3, 0).glyph, std::string(" "));
    screen.field(0, 0, 3, "abcdef", theme::text());
    CHECK_EQ(screen.at(2, 0).glyph, std::string("c"));
    CHECK_EQ(screen.at(3, 0).glyph, std::string(" "));
}

TEST_CASE("screen: box draws corners and title")
{
    ScreenBuffer screen(12, 4);
    screen.box({0, 0, 12, 4}, "LOG", theme::border(), theme::title());
    CHECK_EQ(screen.at(0, 0).glyph, std::string("┌"));
    CHECK_EQ(screen.at(11, 3).glyph, std::string("┘"));
    CHECK_EQ(screen.at(3, 0).glyph, std::string("L"));
    ScreenBuffer::setAsciiBorders(true);
    screen.box({0, 0, 12, 4}, "", theme::border(), theme::title());
    CHECK_EQ(screen.at(0, 0).glyph, std::string("+"));
    ScreenBuffer::setAsciiBorders(false);
}

TEST_CASE("renderer: only changed rows are redrawn")
{
    ScreenBuffer screen(6, 3);
    AnsiRenderer renderer;
    screen.text(0, 0, "top", theme::text());
    screen.text(0, 2, "low", theme::text());
    const std::string first = renderer.render(screen);
    CHECK(first.find("top") != std::string::npos);
    CHECK(first.find("low") != std::string::npos);

    screen.text(0, 2, "new", theme::text());
    const std::string second = renderer.render(screen);
    CHECK(second.find("new") != std::string::npos);
    CHECK(second.find("top") == std::string::npos);
    CHECK(second.find("\x1b[3;1H") != std::string::npos);
}

TEST_CASE("renderer: emits 256-colour styles and cursor position")
{
    ScreenBuffer screen(4, 1);
    screen.text(0, 0, "x", theme::topBar());
    screen.setCursor(2, 0);
    AnsiRenderer renderer;
    const std::string out = renderer.render(screen);
    CHECK(out.find("38;5;255") != std::string::npos);
    CHECK(out.find("48;5;24") != std::string::npos);
    CHECK(out.find("\x1b[1;3H\x1b[?25h") != std::string::npos);
}
