#include "TestFramework.hpp"

#include "editor/Command.hpp"
#include "editor/KmpSearch.hpp"
#include "editor/Motions.hpp"
#include "editor/UndoManager.hpp"

#include <memory>

using namespace ovc::editor;

namespace {

std::unique_ptr<Command> typed(std::size_t position, const std::string& text)
{
    return std::make_unique<InsertTextCommand>(position, text, Origin::typed());
}

} // namespace

TEST_CASE("undo: consecutive typing is one undo step")
{
    TextBuffer buffer;
    UndoManager undo;
    std::size_t cursor = 0;
    for (char c : std::string("hello"))
        cursor = undo.execute(typed(cursor, std::string(1, c)), buffer);
    CHECK_EQ(buffer.text(), std::string("hello"));
    CHECK(undo.undo(buffer).has_value());
    CHECK_EQ(buffer.text(), std::string(""));
    CHECK(!undo.canUndo());
}

TEST_CASE("undo: a new line starts a new undo step")
{
    TextBuffer buffer;
    UndoManager undo;
    std::size_t cursor = undo.execute(typed(0, "ab"), buffer);
    cursor = undo.execute(typed(cursor, "\n"), buffer);
    undo.execute(typed(cursor, "cd"), buffer);
    undo.undo(buffer);
    CHECK_EQ(buffer.text(), std::string("ab\n"));
}

TEST_CASE("undo: closing the group stops merging")
{
    TextBuffer buffer;
    UndoManager undo;
    undo.execute(typed(0, "ab"), buffer);
    undo.closeGroup();
    undo.execute(typed(2, "cd"), buffer);
    undo.undo(buffer);
    CHECK_EQ(buffer.text(), std::string("ab"));
}

TEST_CASE("undo: delete restores text with its original origins")
{
    TextBuffer buffer;
    UndoManager undo;
    undo.execute(typed(0, "abc"), buffer);
    undo.execute(std::make_unique<InsertTextCommand>(3, "XYZ", Origin::paste(OriginKind::Outside, 4)), buffer);
    undo.execute(std::make_unique<DeleteRangeCommand>(1, 4), buffer);
    CHECK_EQ(buffer.text(), std::string("aZ"));

    undo.undo(buffer);
    CHECK_EQ(buffer.text(), std::string("abcXYZ"));
    CHECK(buffer.origins().at(4) == Origin::paste(OriginKind::Outside, 4));
    CHECK(buffer.origins().at(1) == Origin::typed());
}

TEST_CASE("undo: redo replays and new edits clear redo")
{
    TextBuffer buffer;
    UndoManager undo;
    undo.execute(typed(0, "one"), buffer);
    undo.undo(buffer);
    CHECK(undo.canRedo());
    undo.redo(buffer);
    CHECK_EQ(buffer.text(), std::string("one"));
    undo.undo(buffer);
    undo.execute(typed(0, "two"), buffer);
    CHECK(!undo.canRedo());
}

TEST_CASE("undo: composite command undoes all parts")
{
    TextBuffer buffer;
    UndoManager undo;
    undo.execute(typed(0, "old line"), buffer);
    auto change = std::make_unique<CompositeCommand>();
    change->add(std::make_unique<DeleteRangeCommand>(0, 8));
    change->add(typed(0, "new"));
    undo.closeGroup();
    undo.execute(std::move(change), buffer);
    CHECK_EQ(buffer.text(), std::string("new"));
    undo.undo(buffer);
    CHECK_EQ(buffer.text(), std::string("old line"));
}

TEST_CASE("kmp: finds overlapping matches")
{
    KmpSearch search("aa");
    const auto matches = search.findAll("aaaa");
    CHECK_EQ(matches.size(), std::size_t(3));
    KmpSearch abc("abcab");
    CHECK_EQ(abc.findAll("xxabcabcabyy").size(), std::size_t(2));
}

TEST_CASE("kmp: next and previous wrap around")
{
    KmpSearch search("st");
    const std::string text = "stack st.push";
    CHECK_EQ(*search.findNext(text, 1), std::size_t(6));
    CHECK_EQ(*search.findNext(text, 7), std::size_t(0));
    CHECK_EQ(*search.findPrevious(text, 6), std::size_t(0));
    CHECK(!KmpSearch("zz").findNext(text, 0).has_value());
}

TEST_CASE("motions: words")
{
    TextBuffer buffer;
    buffer.insert(0, "int q; cin >> q;", Origin::typed());
    CHECK_EQ(motions::wordForward(buffer, 0, 1), std::size_t(4));  // "q"
    CHECK_EQ(motions::wordForward(buffer, 4, 1), std::size_t(5));  // ";"
    CHECK_EQ(motions::wordForward(buffer, 0, 3), std::size_t(7));  // "cin"
    CHECK_EQ(motions::wordBackward(buffer, 7, 1), std::size_t(5)); // ";"
    CHECK_EQ(motions::wordEnd(buffer, 0, 1), std::size_t(2));      // end of "int"
}

TEST_CASE("motions: lines and vertical movement")
{
    TextBuffer buffer;
    buffer.insert(0, "    first\nab\n  third line", Origin::typed());
    CHECK_EQ(motions::firstNonBlank(buffer, 0), std::size_t(4));
    CHECK_EQ(motions::lineEnd(buffer, 0), std::size_t(9));
    const std::size_t down = motions::vertical(buffer, 7, 1, 7);
    CHECK_EQ(down, std::size_t(12)); // clamped to the end of "ab"
    const std::size_t further = motions::vertical(buffer, down, 1, 7);
    CHECK_EQ(buffer.positionOf(further).column, std::size_t(7));
    CHECK_EQ(motions::indentationOf(buffer, further), std::string("  "));
    CHECK_EQ(motions::right(buffer, 0, 100), std::size_t(9));
}
