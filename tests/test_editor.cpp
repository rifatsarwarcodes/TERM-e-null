#include "TestFramework.hpp"

#include "editor/Editor.hpp"
#include "editor/NormalCommandParser.hpp"
#include "ui/EditorView.hpp"

#include <string>
#include <vector>

using namespace ovc::editor;
using ovc::platform::KeyCode;
using ovc::platform::KeyEvent;

namespace {

// Feeds keys to the editor. Inside the text, "<esc>", "<cr>", "<bs>" and
// "<c-r>" stand for special keys.
void keys(Editor& editor, const std::string& sequence)
{
    for (std::size_t i = 0; i < sequence.size(); ++i) {
        if (sequence[i] == '<') {
            const std::size_t end = sequence.find('>', i);
            const std::string name = sequence.substr(i + 1, end - i - 1);
            if (name == "esc") editor.handleKey(KeyEvent::special(KeyCode::Escape));
            else if (name == "cr") editor.handleKey(KeyEvent::special(KeyCode::Enter));
            else if (name == "bs") editor.handleKey(KeyEvent::special(KeyCode::Backspace));
            else if (name == "tab") editor.handleKey(KeyEvent::special(KeyCode::Tab));
            else if (name == "c-r") editor.handleKey(KeyEvent::control('r'));
            else if (name == "c-z") editor.handleKey(KeyEvent::control('z'));
            else if (name == "up") editor.handleKey(KeyEvent::special(KeyCode::Up));
            i = end;
        } else {
            editor.handleKey(KeyEvent::character(std::string(1, sequence[i])));
        }
    }
}

void prepare(Editor& editor, const std::string& text)
{
    editor.loadText(text, Origin::loaded());
    editor.setMode(ModeId::Normal);
}

struct RecordingHost : IEditorHost {
    std::vector<std::string> commands;
    void onExCommand(const std::string& name, const std::string& argument) override
    {
        commands.push_back(argument.empty() ? name : name + " " + argument);
    }
};

} // namespace

TEST_CASE("parser: counts, operators and motions")
{
    NormalCommandParser parser;
    CHECK(parser.feed('3') == ParseStatus::Pending);
    CHECK(parser.feed('d') == ParseStatus::Pending);
    CHECK(parser.feed('2') == ParseStatus::Pending);
    CHECK(parser.feed('w') == ParseStatus::Complete);
    CHECK_EQ(parser.command().count, std::size_t(6));
    CHECK_EQ(parser.command().op, 'd');
    CHECK_EQ(parser.command().motion, std::string("w"));

    parser.reset();
    parser.feed('d');
    CHECK(parser.feed('d') == ParseStatus::Complete);
    CHECK_EQ(parser.command().motion, std::string("line"));

    parser.reset();
    parser.feed('g');
    CHECK(parser.feed('g') == ParseStatus::Complete);
    CHECK_EQ(parser.command().motion, std::string("gg"));

    parser.reset();
    CHECK(parser.feed('0') == ParseStatus::Complete); // "0" alone is a motion
    parser.reset();
    CHECK(parser.feed('q') == ParseStatus::Invalid);
}

TEST_CASE("editor: starts in insert mode and types text")
{
    Editor editor;
    CHECK(editor.modeId() == ModeId::Insert);
    keys(editor, "int x;");
    CHECK_EQ(editor.buffer().text(), std::string("int x;"));
    CHECK(editor.modified());
}

TEST_CASE("editor: enter keeps indentation and indents after a brace")
{
    Editor editor;
    keys(editor, "int main() {<cr>return 0;<cr>}");
    CHECK_EQ(editor.buffer().text(), std::string("int main() {\n    return 0;\n}"));
}

TEST_CASE("editor: backspace removes a whole indent step")
{
    Editor editor;
    keys(editor, "<tab><bs>x");
    CHECK_EQ(editor.buffer().text(), std::string("x"));
}

TEST_CASE("editor: dd, p and u")
{
    Editor editor;
    prepare(editor, "one\ntwo\nthree");
    keys(editor, "jdd");
    CHECK_EQ(editor.buffer().text(), std::string("one\nthree"));
    CHECK(editor.yankRegister().linewise);
    keys(editor, "p");
    CHECK_EQ(editor.buffer().text(), std::string("one\nthree\ntwo"));
    keys(editor, "uu");
    CHECK_EQ(editor.buffer().text(), std::string("one\ntwo\nthree"));
    keys(editor, "<c-r>");
    CHECK_EQ(editor.buffer().text(), std::string("one\nthree"));
}

TEST_CASE("editor: deleting the last line removes its newline")
{
    Editor editor;
    prepare(editor, "a\nb\nc");
    keys(editor, "Gdd");
    CHECK_EQ(editor.buffer().text(), std::string("a\nb"));
    keys(editor, "ggd2j");
    CHECK_EQ(editor.buffer().text(), std::string(""));
}

TEST_CASE("editor: dw, cw and counts")
{
    Editor editor;
    prepare(editor, "int alpha beta;");
    keys(editor, "wdw");
    CHECK_EQ(editor.buffer().text(), std::string("int beta;"));
    keys(editor, "cwgamma<esc>");
    CHECK_EQ(editor.buffer().text(), std::string("int gamma;"));
    keys(editor, "0" "2x");
    CHECK_EQ(editor.buffer().text(), std::string("t gamma;"));
}

TEST_CASE("editor: yy and P put lines above")
{
    Editor editor;
    prepare(editor, "first\nsecond");
    keys(editor, "jyykP");
    CHECK_EQ(editor.buffer().text(), std::string("second\nfirst\nsecond"));
}

TEST_CASE("editor: o opens a line with the same indentation")
{
    Editor editor;
    prepare(editor, "    x = 1;");
    keys(editor, "oy = 2;<esc>");
    CHECK_EQ(editor.buffer().text(), std::string("    x = 1;\n    y = 2;"));
}

TEST_CASE("editor: cc keeps the indentation")
{
    Editor editor;
    prepare(editor, "    old();");
    keys(editor, "ccnew();<esc>");
    CHECK_EQ(editor.buffer().text(), std::string("    new();"));
}

TEST_CASE("editor: search moves to matches and wraps")
{
    Editor editor;
    prepare(editor, "st.push(c);\nst.pop();\nreturn st.empty();");
    keys(editor, "/st.p<cr>");
    CHECK_EQ(editor.cursorPosition().line, std::size_t(1));
    keys(editor, "n");
    CHECK_EQ(editor.cursorPosition().line, std::size_t(0));
    keys(editor, "/zzz<cr>");
    CHECK(editor.message().find("not found") != std::string::npos);
}

TEST_CASE("editor: colon commands go to the host, numbers go to lines")
{
    Editor editor;
    prepare(editor, "a\nb\nc\nd");
    RecordingHost host;
    editor.setHost(&host);
    keys(editor, ":3<cr>");
    CHECK_EQ(editor.cursorPosition().line, std::size_t(2));
    keys(editor, ":run in.txt<cr>");
    CHECK_EQ(host.commands.size(), std::size_t(1));
    CHECK_EQ(host.commands[0], std::string("run in.txt"));
    editor.setMode(ModeId::Insert);
    editor.handleKey(KeyEvent::control('s'));
    CHECK_EQ(host.commands.back(), std::string("w"));
}

TEST_CASE("editor: prompt answers reach the callback")
{
    Editor editor;
    std::string answer;
    editor.startPrompt("Passcode: ", true, [&](const std::string& text) { answer = text; });
    CHECK(editor.promptActive());
    keys(editor, "abc<bs>d<cr>");
    CHECK_EQ(answer, std::string("abd"));
    CHECK(editor.modeId() == ModeId::Insert); // back to the mode it came from
}

TEST_CASE("editor: pasted text is one undo step with its origin")
{
    Editor editor;
    keys(editor, "x");
    editor.insertPaste("int a;\nint b;", Origin::paste(OriginKind::Outside, 3));
    CHECK(editor.buffer().origins().at(3) == Origin::paste(OriginKind::Outside, 3));
    keys(editor, "<c-z>");
    CHECK_EQ(editor.buffer().text(), std::string("x"));
}

TEST_CASE("editor: read-only blocks edits but not movement")
{
    Editor editor;
    prepare(editor, "abc\ndef");
    editor.setReadOnly(true);
    keys(editor, "jx");
    CHECK_EQ(editor.buffer().text(), std::string("abc\ndef"));
    CHECK_EQ(editor.cursorPosition().line, std::size_t(1));
}

TEST_CASE("view: draws numbers, text and cursor")
{
    Editor editor;
    keys(editor, "int a;\n");
    ovc::ui::ScreenBuffer screen(30, 5);
    ovc::ui::EditorView view;
    view.draw(screen, {0, 0, 30, 5}, editor, true);
    CHECK_EQ(screen.at(3, 0).glyph, std::string("1"));
    CHECK_EQ(screen.at(5, 0).glyph, std::string("i"));
    CHECK(screen.cursorVisible());
    CHECK_EQ(ovc::ui::displayColumn("\tab", 2), std::size_t(5));
}
