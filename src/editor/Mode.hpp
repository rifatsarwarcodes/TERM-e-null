#pragma once

#include "editor/NormalCommandParser.hpp"
#include "platform/InputEvent.hpp"

#include <functional>
#include <string>

namespace ovc::editor {

class Editor;

enum class ModeId { Insert, Normal, Prompt };

// One editing mode (State pattern). The Editor forwards each key to the
// current mode, which decides what it means.
class Mode {
public:
    virtual ~Mode() = default;
    virtual ModeId id() const = 0;
    virtual const char* name() const = 0;
    virtual void handleKey(Editor& editor, const platform::KeyEvent& key) = 0;
};

// Typing inserts text. Arrows, Home/End and Backspace work as expected.
class InsertMode final : public Mode {
public:
    ModeId id() const override { return ModeId::Insert; }
    const char* name() const override { return "INSERT"; }
    void handleKey(Editor& editor, const platform::KeyEvent& key) override;
};

// Vim-style commands: [count][operator][count][motion].
class NormalMode final : public Mode {
public:
    ModeId id() const override { return ModeId::Normal; }
    const char* name() const override { return "NORMAL"; }
    void handleKey(Editor& editor, const platform::KeyEvent& key) override;

    const std::string& pendingKeys() const { return parser_.pendingKeys(); }

private:
    void run(Editor& editor, const NormalCommand& command);
    void runAction(Editor& editor, const NormalCommand& command);
    void runMotion(Editor& editor, const NormalCommand& command);
    void runOperator(Editor& editor, const NormalCommand& command);

    NormalCommandParser parser_;
};

// A one-line prompt at the bottom: ":" commands, "/" search, or a question
// asked by the application (passcode, paste note, confirmation).
class PromptMode final : public Mode {
public:
    using Callback = std::function<void(const std::string&)>;

    ModeId id() const override { return ModeId::Prompt; }
    const char* name() const override { return "COMMAND"; }
    void handleKey(Editor& editor, const platform::KeyEvent& key) override;

    void begin(std::string label, bool masked, Callback onAccept, Callback onCancel, ModeId returnTo);
    void appendText(const std::string& text);

    const std::string& label() const { return label_; }
    const std::string& text() const { return text_; }
    bool masked() const { return masked_; }

private:
    void finish(Editor& editor, bool accepted);

    std::string label_;
    std::string text_;
    bool masked_ = false;
    Callback onAccept_;
    Callback onCancel_;
    ModeId returnTo_ = ModeId::Normal;
};

} // namespace ovc::editor
