#pragma once

#include <string>
#include <variant>

namespace ovc::platform {

enum class KeyCode {
    Char, // a printable character (see KeyEvent::text) or Ctrl/Alt + letter
    Enter,
    Tab,
    BackTab,
    Backspace,
    Delete,
    Insert,
    Escape,
    Up,
    Down,
    Left,
    Right,
    Home,
    End,
    PageUp,
    PageDown,
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
};

struct KeyEvent {
    KeyCode code = KeyCode::Char;
    std::string text; // UTF-8 character for KeyCode::Char, lowercase letter when ctrl is set
    bool ctrl = false;
    bool alt = false;
    bool shift = false;

    static KeyEvent character(std::string utf8) { return {KeyCode::Char, std::move(utf8), false, false, false}; }
    static KeyEvent control(char letter) { return {KeyCode::Char, std::string(1, letter), true, false, false}; }
    static KeyEvent special(KeyCode code) { return {code, {}, false, false, false}; }

    bool isPrintable() const { return code == KeyCode::Char && !ctrl && !alt && !text.empty(); }
    bool is(KeyCode other) const { return code == other && !ctrl && !alt; }
    bool isChar(char c) const { return isPrintable() && text.size() == 1 && text[0] == c; }
    bool isCtrl(char letter) const { return code == KeyCode::Char && ctrl && text.size() == 1 && text[0] == letter; }
};

// Pasted text. `bracketed` is true when the terminal marked it with
// bracketed-paste markers (certain); false when it was recognised from a
// burst of keys arriving together (probable).
struct PasteEvent {
    std::string text;
    bool bracketed = true;
};

struct FocusEvent {
    bool gained = true;
};

struct ResizeEvent {};

using InputEvent = std::variant<KeyEvent, PasteEvent, FocusEvent, ResizeEvent>;

} // namespace ovc::platform
