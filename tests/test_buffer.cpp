#include "TestFramework.hpp"

#include "editor/GapBuffer.hpp"
#include "editor/OriginMap.hpp"
#include "editor/TextBuffer.hpp"

#include <string>
#include <vector>

using namespace ovc::editor;

TEST_CASE("gap buffer: insert at start, middle and end")
{
    GapBuffer buffer(4);
    buffer.insert(0, "ace");
    buffer.insert(1, "b");
    buffer.insert(3, "d");
    buffer.insert(5, "fgh");
    buffer.insert(0, ">");
    CHECK_EQ(buffer.toString(), std::string(">abcdefgh"));
    CHECK_EQ(buffer.size(), std::size_t(9));
    CHECK_EQ(buffer.at(4), 'd');
}

TEST_CASE("gap buffer: erase and substr across the gap")
{
    GapBuffer buffer(2);
    buffer.insert(0, "hello world");
    buffer.insert(5, ",");
    buffer.erase(0, 1);
    CHECK_EQ(buffer.toString(), std::string("ello, world"));
    CHECK_EQ(buffer.substr(3, 5), std::string("o, wo"));
    buffer.erase(4, 7);
    CHECK_EQ(buffer.toString(), std::string("ello"));
}

TEST_CASE("gap buffer: many single character inserts")
{
    GapBuffer buffer(1);
    std::string expected;
    for (int i = 0; i < 500; ++i) {
        const char c = static_cast<char>('a' + i % 26);
        buffer.insert(buffer.size() / 2, std::string(1, c));
        expected.insert(expected.size() / 2, 1, c);
    }
    CHECK_EQ(buffer.toString(), expected);
}

TEST_CASE("origin map: typed text merges into one span")
{
    OriginMap map;
    for (std::size_t i = 0; i < 10; ++i)
        map.insert(i, 1, Origin::typed());
    CHECK_EQ(map.spans().size(), std::size_t(1));
    CHECK_EQ(map.size(), std::size_t(10));
}

TEST_CASE("origin map: paste in the middle splits and erase rejoins")
{
    OriginMap map;
    map.insert(0, 10, Origin::typed());
    map.insert(5, 4, Origin::paste(OriginKind::Outside, 7));
    CHECK_EQ(map.spans().size(), std::size_t(3));
    CHECK(map.at(6) == Origin::paste(OriginKind::Outside, 7));
    CHECK_EQ(map.count(Origin::paste(OriginKind::Outside, 7)), std::size_t(4));

    map.erase(5, 4);
    CHECK_EQ(map.spans().size(), std::size_t(1));
    CHECK_EQ(map.size(), std::size_t(10));
}

TEST_CASE("origin map: slice returns the origins of a range")
{
    OriginMap map;
    map.insert(0, 3, Origin::typed());
    map.insert(3, 3, Origin::paste(OriginKind::Self, 2));
    const auto parts = map.slice(2, 3);
    CHECK_EQ(parts.size(), std::size_t(2));
    CHECK_EQ(parts[0].length, std::size_t(1));
    CHECK_EQ(parts[1].length, std::size_t(2));
}

TEST_CASE("origin: encode and decode")
{
    CHECK_EQ(Origin::paste(OriginKind::Outside, 24).encode(), std::string("O#24"));
    CHECK_EQ(Origin::typed().encode(), std::string("T"));
    CHECK(Origin::decode("S#3") == Origin::paste(OriginKind::Self, 3));
    CHECK(Origin::decode("L") == Origin::loaded());
}

TEST_CASE("text buffer: lines and positions")
{
    TextBuffer buffer;
    CHECK_EQ(buffer.lineCount(), std::size_t(1));
    buffer.insert(0, "int main() {\n    return 0;\n}", Origin::typed());
    CHECK_EQ(buffer.lineCount(), std::size_t(3));
    CHECK_EQ(buffer.line(1), std::string("    return 0;"));
    CHECK_EQ(buffer.lineLength(2), std::size_t(1));

    const auto position = buffer.positionOf(buffer.lineStart(1) + 4);
    CHECK_EQ(position.line, std::size_t(1));
    CHECK_EQ(position.column, std::size_t(4));
    CHECK_EQ(buffer.offsetOf(0, 100), std::size_t(12)); // clamped to line end
}

namespace {

struct RecordingObserver : IEditObserver {
    std::vector<std::string> log;
    void onInsert(std::size_t position, const std::string& text, const Origin& origin) override
    {
        log.push_back("+" + std::to_string(position) + ":" + text + ":" + origin.encode());
    }
    void onErase(std::size_t position, const std::string& removed) override
    {
        log.push_back("-" + std::to_string(position) + ":" + removed);
    }
};

} // namespace

TEST_CASE("text buffer: observers see every change")
{
    TextBuffer buffer;
    RecordingObserver observer;
    buffer.addObserver(&observer);
    buffer.insert(0, "abc", Origin::typed());
    buffer.insert(1, "XY", Origin::paste(OriginKind::Outside, 1));
    CHECK_EQ(buffer.erase(0, 2), std::string("aX"));
    buffer.removeObserver(&observer);
    buffer.insert(0, "z", Origin::typed());

    CHECK_EQ(observer.log.size(), std::size_t(3));
    CHECK_EQ(observer.log[1], std::string("+1:XY:O#1"));
    CHECK_EQ(observer.log[2], std::string("-0:aX"));
    CHECK_EQ(buffer.text(), std::string("zYbc"));
}
