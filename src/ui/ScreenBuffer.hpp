#pragma once

#include "ui/Style.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace ovc::ui {

struct Cell {
    std::string glyph = " "; // one UTF-8 code point
    Style style;

    friend bool operator==(const Cell& a, const Cell& b) { return a.glyph == b.glyph && a.style == b.style; }
    friend bool operator!=(const Cell& a, const Cell& b) { return !(a == b); }
};

// One frame of the screen as a grid of styled cells. Screens draw into it;
// AnsiRenderer turns it into terminal output. Everything outside the grid
// is clipped, so drawing code never has to check bounds.
class ScreenBuffer {
public:
    ScreenBuffer(int width = 0, int height = 0);

    void resize(int width, int height);
    void clear(const Style& style = {});

    int width() const { return width_; }
    int height() const { return height_; }
    Rect bounds() const { return {0, 0, width_, height_}; }

    const Cell& at(int x, int y) const;

    void put(int x, int y, std::string_view glyph, const Style& style);
    // Writes UTF-8 text from (x, y); returns the number of columns used.
    int text(int x, int y, std::string_view utf8, const Style& style, int maxWidth = -1);
    // Like text() but pads with spaces to exactly `width` columns.
    void field(int x, int y, int width, std::string_view utf8, const Style& style);
    void fill(const Rect& area, const Style& style, std::string_view glyph = " ");
    // Changes the style of existing cells, keeping their characters.
    void restyle(int x, int y, int width, const Style& style);
    void setBackground(int x, int y, int width, Color background);
    void box(const Rect& area, std::string_view title, const Style& borderStyle, const Style& titleStyle);
    void horizontalLine(int x, int y, int width, const Style& style);

    void setCursor(int x, int y) { cursorX_ = x, cursorY_ = y, cursorVisible_ = true; }
    void hideCursor() { cursorVisible_ = false; }
    bool cursorVisible() const { return cursorVisible_; }
    int cursorX() const { return cursorX_; }
    int cursorY() const { return cursorY_; }

    // Use + - | instead of box-drawing characters (for old consoles).
    static void setAsciiBorders(bool ascii);

private:
    bool inside(int x, int y) const { return x >= 0 && y >= 0 && x < width_ && y < height_; }
    Cell& cellAt(int x, int y) { return cells_[static_cast<std::size_t>(y * width_ + x)]; }

    int width_ = 0;
    int height_ = 0;
    std::vector<Cell> cells_;
    int cursorX_ = 0;
    int cursorY_ = 0;
    bool cursorVisible_ = false;
};

} // namespace ovc::ui
