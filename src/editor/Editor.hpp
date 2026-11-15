#pragma once

#include "editor/Mode.hpp"
#include "editor/TextBuffer.hpp"
#include "editor/UndoManager.hpp"
#include "platform/InputEvent.hpp"

#include <cstddef>
#include <string>
#include <string_view>

namespace ovc::editor {

// What y/d put aside and p/P put back.
struct Register {
    std::string text;
    bool linewise = false;
};

// The application side of the editor: ":" commands it does not handle
// itself, and pastes made with the editor's own p / P.
class IEditorHost {
public:
    virtual ~IEditorHost() = default;
    virtual void onExCommand(const std::string& name, const std::string& argument) = 0;
    virtual Origin onInsidePaste(const std::string& text)
    {
        (void)text;
        return Origin::paste(OriginKind::Inside, 0);
    }
};

// The editing engine: document, cursor, modes, undo, register and search.
// It knows nothing about terminals or exams; keys come in through
// handleKey() and the application reacts through IEditorHost.
class Editor {
public:
    Editor();
    // The editor points at its own mode objects, so it is never copied or moved.
    Editor(const Editor&) = delete;
    Editor& operator=(const Editor&) = delete;

    // --- document -------------------------------------------------------
    TextBuffer& buffer() { return buffer_; }
    const TextBuffer& buffer() const { return buffer_; }
    // Initial content of the file. Not undoable.
    void loadText(const std::string& text, const Origin& origin);
    bool modified() const { return revision_ != savedRevision_; }
    void markSaved() { savedRevision_ = revision_; }

    // --- cursor ---------------------------------------------------------
    std::size_t cursor() const { return cursor_; }
    TextPosition cursorPosition() const { return buffer_.positionOf(cursor_); }
    void moveCursorTo(std::size_t offset);
    void moveVertically(long lines);
    void gotoLine(std::size_t lineNumber); // 1-based

    // --- modes and input ------------------------------------------------
    ModeId modeId() const { return mode_->id(); }
    const char* modeName() const { return mode_->name(); }
    void setMode(ModeId id);
    std::string pendingKeys() const;

    void handleKey(const platform::KeyEvent& key);
    // Inserts pasted text at the cursor as one undo step.
    void insertPaste(const std::string& text, const Origin& origin);

    // --- editing operations (used by the modes) -------------------------
    void typeText(std::string_view text);
    void insertNewline();
    void insertTab();
    void backspace();
    void deleteForward();
    void deleteRange(std::size_t from, std::size_t to);
    void yankRange(std::size_t from, std::size_t to);
    void deleteLines(std::size_t firstLine, std::size_t lastLine);
    void yankLines(std::size_t firstLine, std::size_t lastLine);
    void changeLines(std::size_t firstLine, std::size_t lastLine);
    void putRegister(bool before, std::size_t count);
    void openLine(bool above);
    void undo();
    void redo();
    void closeUndoGroup() { undo_.closeGroup(); }

    // --- prompt, commands and search ------------------------------------
    void startPrompt(std::string label, bool masked, PromptMode::Callback onAccept,
        PromptMode::Callback onCancel = {});
    bool promptActive() const { return mode_ == &promptMode_; }
    const PromptMode& prompt() const { return promptMode_; }
    void appendToPrompt(const std::string& text) { promptMode_.appendText(text); }
    void startCommandLine();
    void startSearch();
    void executeCommandLine(const std::string& line);
    void search(const std::string& pattern, bool forward);
    void repeatSearch(bool reverse);
    const std::string& searchPattern() const { return searchPattern_; }

    // --- status ---------------------------------------------------------
    void setMessage(std::string message) { message_ = std::move(message); }
    const std::string& message() const { return message_; }
    void setHost(IEditorHost* host) { host_ = host; }
    void setReadOnly(bool readOnly) { readOnly_ = readOnly; }
    bool readOnly() const { return readOnly_; }
    const Register& yankRegister() const { return register_; }

private:
    bool canWrite();
    void run(std::unique_ptr<Command> command);
    bool handleNavigationKey(const platform::KeyEvent& key);
    std::string linesText(std::size_t firstLine, std::size_t lastLine) const;

    TextBuffer buffer_;
    UndoManager undo_;
    std::size_t cursor_ = 0;
    std::size_t preferredColumn_ = 0;

    InsertMode insertMode_;
    NormalMode normalMode_;
    PromptMode promptMode_;
    Mode* mode_;

    Register register_;
    std::string searchPattern_;
    bool lastSearchForward_ = true;
    std::string message_;
    IEditorHost* host_ = nullptr;
    bool readOnly_ = false;
    std::size_t revision_ = 0;
    std::size_t savedRevision_ = 0;
};

} // namespace ovc::editor
