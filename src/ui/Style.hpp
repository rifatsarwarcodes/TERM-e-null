#pragma once

namespace ovc::ui {

// A 256-colour palette index; -1 means the terminal's default colour.
struct Color {
    int index = -1;
    friend bool operator==(Color a, Color b) { return a.index == b.index; }
    friend bool operator!=(Color a, Color b) { return a.index != b.index; }
};

struct Style {
    Color foreground;
    Color background;
    bool bold = false;
    bool underline = false;
    bool reverse = false;

    Style withBackground(Color color) const
    {
        Style copy = *this;
        copy.background = color;
        return copy;
    }
    Style withForeground(Color color) const
    {
        Style copy = *this;
        copy.foreground = color;
        return copy;
    }
    Style withBold(bool value = true) const
    {
        Style copy = *this;
        copy.bold = value;
        return copy;
    }

    friend bool operator==(const Style& a, const Style& b)
    {
        return a.foreground == b.foreground && a.background == b.background && a.bold == b.bold
            && a.underline == b.underline && a.reverse == b.reverse;
    }
    friend bool operator!=(const Style& a, const Style& b) { return !(a == b); }
};

struct Rect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    int right() const { return x + width; }
    int bottom() const { return y + height; }
    bool empty() const { return width <= 0 || height <= 0; }
    // The area inside a one-character border.
    Rect inner() const { return {x + 1, y + 1, width - 2, height - 2}; }
};

} // namespace ovc::ui
