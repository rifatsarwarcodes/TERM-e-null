#include "common/Strings.hpp"
#include "editor/Editor.hpp"
#include "editor/Motions.hpp"

#include <algorithm>

namespace ovc::editor {

using platform::KeyCode;
using platform::KeyEvent;

// ---------------------------------------------------------------- Insert

void InsertMode::handleKey(Editor& editor, const KeyEvent& key)
{
    if (key.isPrintable()) {
        editor.typeText(key.text);
        return;
    }
    switch (key.code) {
    case KeyCode::Enter: editor.insertNewline(); break;
    case KeyCode::Tab: editor.insertTab(); break;
    case KeyCode::Backspace: editor.backspace(); break;
    case KeyCode::Delete: editor.deleteForward(); break;
    case KeyCode::Escape: editor.setMode(ModeId::Normal); break;
    default: break;
    }
}

// ---------------------------------------------------------------- Normal

namespace {

bool isLinewiseMotion(const std::string& motion)
{
    return motion == "j" || motion == "k" || motion == "G" || motion == "gg" || motion == "line";
}

// Where a motion lands from the current cursor.
std::size_t motionTarget(const Editor& editor, const NormalCommand& command)
{
    const TextBuffer& buffer = editor.buffer();
    const std::size_t cursor = editor.cursor();
    const std::size_t count = command.count;
    const std::string& motion = command.motion;

    if (motion == "h") return motions::left(buffer, cursor, count);
    if (motion == "l") return motions::right(buffer, cursor, count);
    if (motion == "w") return motions::wordForward(buffer, cursor, count);
    if (motion == "b") return motions::wordBackward(buffer, cursor, count);
    if (motion == "e") return motions::wordEnd(buffer, cursor, count);
    if (motion == "0") return motions::lineStart(buffer, cursor);
    if (motion == "^") return motions::firstNonBlank(buffer, cursor);
    if (motion == "$") {
        const std::size_t moved = motions::vertical(buffer, cursor, static_cast<long>(count) - 1, 0);
        return motions::lineEnd(buffer, moved);
    }
    if (motion == "j") return motions::vertical(buffer, cursor, static_cast<long>(count), 0);
    if (motion == "k") return motions::vertical(buffer, cursor, -static_cast<long>(count), 0);
    if (motion == "G" || motion == "gg") {
        std::size_t line = motion == "G" ? buffer.lineCount() - 1 : 0;
        if (command.hasCount)
            line = std::min(count - 1, buffer.lineCount() - 1);
        return motions::firstNonBlank(buffer, buffer.lineStart(line));
    }
    return cursor;
}

} // namespace

void NormalMode::handleKey(Editor& editor, const KeyEvent& key)
{
    if (key.is(KeyCode::Escape)) {
        parser_.reset();
        return;
    }
    if (key.isCtrl('r')) {
        parser_.reset();
        editor.redo();
        return;
    }
    if (key.is(KeyCode::Enter)) {
        parser_.reset();
        editor.moveVertically(1);
        editor.moveCursorTo(motions::firstNonBlank(editor.buffer(), editor.cursor()));
        return;
    }
    if (key.is(KeyCode::Backspace)) {
        parser_.reset();
        if (editor.cursor() > 0)
            editor.moveCursorTo(editor.cursor() - 1);
        return;
    }
    if (key.is(KeyCode::Delete)) {
        parser_.reset();
        editor.deleteForward();
        return;
    }
    if (!key.isPrintable() || key.text.size() != 1)
        return;

    switch (parser_.feed(key.text[0])) {
    case ParseStatus::Pending:
        return;
    case ParseStatus::Invalid:
        parser_.reset();
        return;
    case ParseStatus::Complete: {
        const NormalCommand command = parser_.command();
        parser_.reset();
        run(editor, command);
        return;
    }
    }
}

void NormalMode::run(Editor& editor, const NormalCommand& command)
{
    if (command.action != 0)
        runAction(editor, command);
    else if (command.op != 0)
        runOperator(editor, command);
    else
        runMotion(editor, command);
}

void NormalMode::runMotion(Editor& editor, const NormalCommand& command)
{
    if (command.motion == "j" || command.motion == "k") {
        const long lines = static_cast<long>(command.count);
        editor.moveVertically(command.motion == "j" ? lines : -lines);
        return;
    }
    editor.moveCursorTo(motionTarget(editor, command));
}

void NormalMode::runOperator(Editor& editor, const NormalCommand& command)
{
    const TextBuffer& buffer = editor.buffer();
    const std::size_t cursor = editor.cursor();

    if (isLinewiseMotion(command.motion)) {
        const std::size_t currentLine = buffer.positionOf(cursor).line;
        std::size_t firstLine = currentLine;
        std::size_t lastLine = currentLine;
        if (command.motion == "line") {
            lastLine = std::min(currentLine + command.count - 1, buffer.lineCount() - 1);
        } else {
            const std::size_t targetLine = buffer.positionOf(motionTarget(editor, command)).line;
            firstLine = std::min(currentLine, targetLine);
            lastLine = std::max(currentLine, targetLine);
        }
        switch (command.op) {
        case 'd': editor.deleteLines(firstLine, lastLine); break;
        case 'c': editor.changeLines(firstLine, lastLine); break;
        case 'y':
            editor.yankLines(firstLine, lastLine);
            editor.moveCursorTo(buffer.lineStart(firstLine));
            break;
        default: break;
        }
        return;
    }

    NormalCommand effective = command;
    // Like Vim, "cw" on a word changes to the end of the word.
    const bool onSpace = cursor < buffer.size() && (buffer.at(cursor) == ' ' || buffer.at(cursor) == '\n');
    if (command.op == 'c' && command.motion == "w" && !onSpace)
        effective.motion = "e";

    const std::size_t target = motionTarget(editor, effective);
    std::size_t from = std::min(cursor, target);
    std::size_t to = std::max(cursor, target);
    if (effective.motion == "e" && to < buffer.size())
        ++to; // "e" includes the last character of the word
    if (effective.motion == "w")
        to = std::min(to, std::max(from, motions::lineEnd(buffer, from))); // stay on the line

    switch (command.op) {
    case 'd': editor.deleteRange(from, to); break;
    case 'c':
        editor.deleteRange(from, to);
        editor.setMode(ModeId::Insert);
        break;
    case 'y':
        editor.yankRange(from, to);
        editor.moveCursorTo(from);
        break;
    default: break;
    }
}

void NormalMode::runAction(Editor& editor, const NormalCommand& command)
{
    const TextBuffer& buffer = editor.buffer();
    const std::size_t cursor = editor.cursor();
    const std::size_t count = command.count;

    switch (command.action) {
    case 'x': editor.deleteRange(cursor, motions::right(buffer, cursor, count)); break;
    case 'X': editor.deleteRange(motions::left(buffer, cursor, count), cursor); break;
    case 'p': editor.putRegister(false, count); break;
    case 'P': editor.putRegister(true, count); break;
    case 'i': editor.setMode(ModeId::Insert); break;
    case 'a':
        editor.moveCursorTo(motions::right(buffer, cursor, 1));
        editor.setMode(ModeId::Insert);
        break;
    case 'A':
        editor.moveCursorTo(motions::lineEnd(buffer, cursor));
        editor.setMode(ModeId::Insert);
        break;
    case 'I':
        editor.moveCursorTo(motions::firstNonBlank(buffer, cursor));
        editor.setMode(ModeId::Insert);
        break;
    case 'o': editor.openLine(false); break;
    case 'O': editor.openLine(true); break;
    case 'u':
        for (std::size_t i = 0; i < count; ++i)
            editor.undo();
        break;
    case 'n': editor.repeatSearch(false); break;
    case 'N': editor.repeatSearch(true); break;
    case 'D': editor.deleteRange(cursor, motions::lineEnd(buffer, cursor)); break;
    case 'C':
        editor.deleteRange(cursor, motions::lineEnd(buffer, cursor));
        editor.setMode(ModeId::Insert);
        break;
    case ':': editor.startCommandLine(); break;
    case '/': editor.startSearch(); break;
    default: break;
    }
}

// ---------------------------------------------------------------- Prompt

void PromptMode::begin(std::string label, bool masked, Callback onAccept, Callback onCancel, ModeId returnTo)
{
    label_ = std::move(label);
    text_.clear();
    masked_ = masked;
    onAccept_ = std::move(onAccept);
    onCancel_ = std::move(onCancel);
    returnTo_ = returnTo;
}

void PromptMode::appendText(const std::string& text)
{
    // A prompt is one line: keep only the first line of pasted text.
    text_ += text.substr(0, text.find('\n'));
}

void PromptMode::handleKey(Editor& editor, const KeyEvent& key)
{
    if (key.isPrintable()) {
        text_ += key.text;
        return;
    }
    if (key.isCtrl('u')) {
        text_.clear();
        return;
    }
    switch (key.code) {
    case KeyCode::Enter: finish(editor, true); break;
    case KeyCode::Escape: finish(editor, false); break;
    case KeyCode::Backspace:
        if (text_.empty())
            finish(editor, false);
        else
            strings::popCodePoint(text_);
        break;
    default: break;
    }
}

void PromptMode::finish(Editor& editor, bool accepted)
{
    // Leave the prompt first: the callback may start another prompt.
    const Callback callback = accepted ? onAccept_ : onCancel_;
    const std::string answer = text_;
    text_.clear();
    editor.setMode(returnTo_);
    if (callback)
        callback(answer);
}

} // namespace ovc::editor
