#include "editor/Editor.hpp"

#include "common/Strings.hpp"
#include "editor/KmpSearch.hpp"
#include "editor/Motions.hpp"

#include <algorithm>

namespace ovc::editor {

using platform::KeyCode;
using platform::KeyEvent;

namespace {

constexpr std::size_t IndentWidth = 4;
constexpr long PageLines = 20;

bool isContinuationByte(char c)
{
    return (static_cast<unsigned char>(c) & 0xC0) == 0x80;
}

} // namespace

Editor::Editor()
    : mode_(&insertMode_)
{
}

void Editor::loadText(const std::string& text, const Origin& origin)
{
    buffer_.insert(0, text, origin);
    cursor_ = 0;
    preferredColumn_ = 0;
}

void Editor::moveCursorTo(std::size_t offset)
{
    cursor_ = std::min(offset, buffer_.size());
    preferredColumn_ = buffer_.positionOf(cursor_).column;
    undo_.closeGroup();
}

void Editor::moveVertically(long lines)
{
    cursor_ = motions::vertical(buffer_, cursor_, lines, preferredColumn_);
    undo_.closeGroup();
}

void Editor::gotoLine(std::size_t lineNumber)
{
    const std::size_t line = std::min(lineNumber == 0 ? 0 : lineNumber - 1, buffer_.lineCount() - 1);
    moveCursorTo(motions::firstNonBlank(buffer_, buffer_.lineStart(line)));
}

void Editor::setMode(ModeId id)
{
    undo_.closeGroup();
    switch (id) {
    case ModeId::Insert: mode_ = &insertMode_; break;
    case ModeId::Normal: mode_ = &normalMode_; break;
    case ModeId::Prompt: mode_ = &promptMode_; break;
    }
}

std::string Editor::pendingKeys() const
{
    return mode_ == &normalMode_ ? normalMode_.pendingKeys() : std::string();
}

bool Editor::canWrite()
{
    if (!readOnly_)
        return true;
    message_ = "Editing is locked.";
    return false;
}

void Editor::run(std::unique_ptr<Command> command)
{
    cursor_ = std::min(undo_.execute(std::move(command), buffer_), buffer_.size());
    preferredColumn_ = buffer_.positionOf(cursor_).column;
    ++revision_;
}

bool Editor::handleNavigationKey(const KeyEvent& key)
{
    switch (key.code) {
    case KeyCode::Left:
        if (cursor_ > 0)
            moveCursorTo(cursor_ - 1);
        return true;
    case KeyCode::Right:
        if (cursor_ < buffer_.size())
            moveCursorTo(cursor_ + 1);
        return true;
    case KeyCode::Up: moveVertically(-1); return true;
    case KeyCode::Down: moveVertically(1); return true;
    case KeyCode::PageUp: moveVertically(-PageLines); return true;
    case KeyCode::PageDown: moveVertically(PageLines); return true;
    case KeyCode::Home:
        moveCursorTo(key.ctrl ? 0 : motions::lineStart(buffer_, cursor_));
        return true;
    case KeyCode::End:
        moveCursorTo(key.ctrl ? buffer_.size() : motions::lineEnd(buffer_, cursor_));
        return true;
    default: return false;
    }
}

void Editor::handleKey(const KeyEvent& key)
{
    if (!promptActive()) {
        message_.clear();
        if (key.isCtrl('z')) {
            undo();
            return;
        }
        if (key.isCtrl('y')) {
            redo();
            return;
        }
        if (key.isCtrl('s')) {
            if (host_ != nullptr)
                host_->onExCommand("w", "");
            return;
        }
        // Keep multi-byte UTF-8 characters whole when moving left/right.
        if (key.code == KeyCode::Left || key.code == KeyCode::Right) {
            const bool left = key.code == KeyCode::Left;
            handleNavigationKey(key);
            while (cursor_ > 0 && cursor_ < buffer_.size() && isContinuationByte(buffer_.at(cursor_)))
                moveCursorTo(left ? cursor_ - 1 : cursor_ + 1);
            return;
        }
        if (handleNavigationKey(key))
            return;
    }
    mode_->handleKey(*this, key);
}

void Editor::insertPaste(const std::string& text, const Origin& origin)
{
    if (text.empty() || !canWrite())
        return;
    undo_.closeGroup();
    run(std::make_unique<InsertTextCommand>(cursor_, text, origin));
    undo_.closeGroup();
}

void Editor::typeText(std::string_view text)
{
    if (!canWrite())
        return;
    // Typing '}' on a line that only has indentation removes one level.
    if (text == "}") {
        const std::size_t start = motions::lineStart(buffer_, cursor_);
        const std::string before = buffer_.substr(start, cursor_ - start);
        if (before.size() >= IndentWidth && strings::isBlank(before) && before.find('\t') == std::string::npos)
            run(std::make_unique<DeleteRangeCommand>(cursor_ - IndentWidth, IndentWidth));
    }
    run(std::make_unique<InsertTextCommand>(cursor_, std::string(text), Origin::typed()));
}

void Editor::insertNewline()
{
    if (!canWrite())
        return;
    const std::size_t start = motions::lineStart(buffer_, cursor_);
    const std::string before = buffer_.substr(start, cursor_ - start);
    std::string indent = motions::indentationOf(buffer_, cursor_);
    indent = indent.substr(0, std::min(indent.size(), before.size()));
    const std::string trimmed = strings::trimRight(before);
    if (!trimmed.empty() && trimmed.back() == '{')
        indent += std::string(IndentWidth, ' ');
    run(std::make_unique<InsertTextCommand>(cursor_, "\n" + indent, Origin::typed()));
}

void Editor::insertTab()
{
    if (!canWrite())
        return;
    const std::size_t column = buffer_.positionOf(cursor_).column;
    const std::size_t spaces = IndentWidth - column % IndentWidth;
    run(std::make_unique<InsertTextCommand>(cursor_, std::string(spaces, ' '), Origin::typed()));
}

void Editor::backspace()
{
    if (cursor_ == 0 || !canWrite())
        return;
    const std::size_t start = motions::lineStart(buffer_, cursor_);
    const std::string before = buffer_.substr(start, cursor_ - start);
    std::size_t count = 1;
    if (!before.empty() && before.size() % IndentWidth == 0 && before.find_first_not_of(' ') == std::string::npos)
        count = IndentWidth; // remove a whole indentation step
    else
        while (count < cursor_ && isContinuationByte(buffer_.at(cursor_ - count)))
            ++count;
    run(std::make_unique<DeleteRangeCommand>(cursor_ - count, count));
}

void Editor::deleteForward()
{
    if (cursor_ >= buffer_.size() || !canWrite())
        return;
    std::size_t count = 1;
    while (cursor_ + count < buffer_.size() && isContinuationByte(buffer_.at(cursor_ + count)))
        ++count;
    run(std::make_unique<DeleteRangeCommand>(cursor_, count));
}

void Editor::deleteRange(std::size_t from, std::size_t to)
{
    if (to <= from || !canWrite())
        return;
    yankRange(from, to);
    undo_.closeGroup();
    run(std::make_unique<DeleteRangeCommand>(from, to - from));
    undo_.closeGroup();
}

void Editor::yankRange(std::size_t from, std::size_t to)
{
    if (to <= from)
        return;
    register_ = {buffer_.substr(from, to - from), false};
}

std::string Editor::linesText(std::size_t firstLine, std::size_t lastLine) const
{
    const std::size_t from = buffer_.lineStart(firstLine);
    const std::size_t to = buffer_.lineStart(lastLine) + buffer_.lineLength(lastLine);
    return buffer_.substr(from, to - from) + "\n";
}

void Editor::yankLines(std::size_t firstLine, std::size_t lastLine)
{
    register_ = {linesText(firstLine, lastLine), true};
}

void Editor::deleteLines(std::size_t firstLine, std::size_t lastLine)
{
    if (!canWrite())
        return;
    const std::size_t lines = buffer_.lineCount();
    lastLine = std::min(lastLine, lines - 1);
    yankLines(firstLine, lastLine);

    std::size_t from = buffer_.lineStart(firstLine);
    std::size_t to = lastLine + 1 < lines ? buffer_.lineStart(lastLine + 1) : buffer_.size();
    if (lastLine + 1 >= lines && firstLine > 0)
        from -= 1; // deleting the last lines: also remove the newline before them
    undo_.closeGroup();
    run(std::make_unique<DeleteRangeCommand>(from, to - from));
    undo_.closeGroup();
    const std::size_t line = std::min(firstLine, buffer_.lineCount() - 1);
    moveCursorTo(motions::firstNonBlank(buffer_, buffer_.lineStart(line)));
}

void Editor::changeLines(std::size_t firstLine, std::size_t lastLine)
{
    if (!canWrite())
        return;
    lastLine = std::min(lastLine, buffer_.lineCount() - 1);
    yankLines(firstLine, lastLine);
    const std::size_t from = buffer_.lineStart(firstLine);
    const std::size_t to = buffer_.lineStart(lastLine) + buffer_.lineLength(lastLine);
    const std::string indent = motions::indentationOf(buffer_, from);

    auto change = std::make_unique<CompositeCommand>();
    change->add(std::make_unique<DeleteRangeCommand>(from, to - from));
    change->add(std::make_unique<InsertTextCommand>(from, indent, Origin::typed()));
    undo_.closeGroup();
    run(std::move(change));
    setMode(ModeId::Insert);
}

void Editor::putRegister(bool before, std::size_t count)
{
    if (register_.text.empty() || !canWrite())
        return;
    std::string text;
    for (std::size_t i = 0; i < count; ++i)
        text += register_.text;
    const Origin origin = host_ != nullptr ? host_->onInsidePaste(text) : Origin::paste(OriginKind::Inside, 0);

    undo_.closeGroup();
    if (register_.linewise) {
        std::size_t position;
        if (before) {
            position = motions::lineStart(buffer_, cursor_);
        } else {
            const std::size_t end = motions::lineEnd(buffer_, cursor_);
            if (end >= buffer_.size()) {
                // Last line without a newline: add one before the pasted lines.
                text = "\n" + text.substr(0, text.size() - 1);
                position = end;
            } else {
                position = end + 1;
            }
        }
        run(std::make_unique<InsertTextCommand>(position, text, origin));
        const std::size_t lineStartOffset = text.front() == '\n' ? position + 1 : position;
        moveCursorTo(motions::firstNonBlank(buffer_, lineStartOffset));
    } else {
        std::size_t position = cursor_;
        if (!before && cursor_ < motions::lineEnd(buffer_, cursor_))
            ++position;
        run(std::make_unique<InsertTextCommand>(position, text, origin));
        moveCursorTo(position + text.size() - 1);
    }
    undo_.closeGroup();
}

void Editor::openLine(bool above)
{
    if (!canWrite())
        return;
    const std::string indent = motions::indentationOf(buffer_, cursor_);
    undo_.closeGroup();
    if (above) {
        const std::size_t start = motions::lineStart(buffer_, cursor_);
        run(std::make_unique<InsertTextCommand>(start, indent + "\n", Origin::typed()));
        moveCursorTo(start + indent.size());
    } else {
        run(std::make_unique<InsertTextCommand>(motions::lineEnd(buffer_, cursor_), "\n" + indent, Origin::typed()));
    }
    setMode(ModeId::Insert);
}

void Editor::undo()
{
    if (!canWrite())
        return;
    if (const auto position = undo_.undo(buffer_)) {
        moveCursorTo(*position);
        ++revision_;
    } else {
        message_ = "Nothing to undo.";
    }
}

void Editor::redo()
{
    if (!canWrite())
        return;
    if (const auto position = undo_.redo(buffer_)) {
        moveCursorTo(*position);
        ++revision_;
    } else {
        message_ = "Nothing to redo.";
    }
}

void Editor::startPrompt(std::string label, bool masked, PromptMode::Callback onAccept, PromptMode::Callback onCancel)
{
    const ModeId returnTo = mode_ == &promptMode_ ? ModeId::Normal : mode_->id();
    promptMode_.begin(std::move(label), masked, std::move(onAccept), std::move(onCancel), returnTo);
    mode_ = &promptMode_;
}

void Editor::startCommandLine()
{
    startPrompt(":", false, [this](const std::string& line) { executeCommandLine(line); });
}

void Editor::startSearch()
{
    startPrompt("/", false, [this](const std::string& pattern) { search(pattern, true); });
}

void Editor::executeCommandLine(const std::string& line)
{
    const std::string command = strings::trim(line);
    if (command.empty())
        return;
    std::uint64_t lineNumber = 0;
    if (strings::parseUnsigned(command, lineNumber)) {
        gotoLine(static_cast<std::size_t>(lineNumber));
        return;
    }
    const std::size_t space = command.find(' ');
    const std::string name = command.substr(0, space);
    const std::string argument = space == std::string::npos ? "" : strings::trim(command.substr(space + 1));
    if (host_ != nullptr)
        host_->onExCommand(name, argument);
    else
        message_ = "Not an editor command: " + name;
}

void Editor::search(const std::string& pattern, bool forward)
{
    if (!pattern.empty())
        searchPattern_ = pattern;
    lastSearchForward_ = forward;
    if (searchPattern_.empty()) {
        message_ = "No previous search.";
        return;
    }
    const KmpSearch matcher(searchPattern_);
    const std::string text = buffer_.text();
    const auto match = forward ? matcher.findNext(text, cursor_ + 1) : matcher.findPrevious(text, cursor_);
    if (match)
        moveCursorTo(*match);
    else
        message_ = "Pattern not found: " + searchPattern_;
}

void Editor::repeatSearch(bool reverse)
{
    // N searches the other way without changing the direction n uses.
    const bool direction = lastSearchForward_;
    search("", reverse ? !direction : direction);
    lastSearchForward_ = direction;
}

} // namespace ovc::editor
