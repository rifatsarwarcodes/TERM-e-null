#include "ui/ScreenBuffer.hpp"

namespace ovc::ui {

namespace {

struct BorderGlyphs {
    const char* horizontal;
    const char* vertical;
    const char* topLeft;
    const char* topRight;
    const char* bottomLeft;
    const char* bottomRight;
};

constexpr BorderGlyphs UnicodeBorders{"─", "│", "┌", "┐", "└", "┘"};
constexpr BorderGlyphs AsciiBorders{"-", "|", "+", "+", "+", "+"};
const BorderGlyphs* activeBorders = &UnicodeBorders;

std::size_t codePointLength(unsigned char lead)
{
    if ((lead & 0xE0) == 0xC0) return 2;
    if ((lead & 0xF0) == 0xE0) return 3;
    if ((lead & 0xF8) == 0xF0) return 4;
    return 1;
}

} // namespace

void ScreenBuffer::setAsciiBorders(bool ascii)
{
    activeBorders = ascii ? &AsciiBorders : &UnicodeBorders;
}

ScreenBuffer::ScreenBuffer(int width, int height)
{
    resize(width, height);
}

void ScreenBuffer::resize(int width, int height)
{
    width_ = width < 0 ? 0 : width;
    height_ = height < 0 ? 0 : height;
    cells_.assign(static_cast<std::size_t>(width_ * height_), Cell{});
}

void ScreenBuffer::clear(const Style& style)
{
    for (auto& cell : cells_)
        cell = Cell{" ", style};
    cursorVisible_ = false;
}

const Cell& ScreenBuffer::at(int x, int y) const
{
    static const Cell outside{};
    return inside(x, y) ? cells_[static_cast<std::size_t>(y * width_ + x)] : outside;
}

void ScreenBuffer::put(int x, int y, std::string_view glyph, const Style& style)
{
    if (!inside(x, y))
        return;
    Cell& cell = cellAt(x, y);
    cell.glyph.assign(glyph.data(), glyph.size());
    cell.style = style;
}

int ScreenBuffer::text(int x, int y, std::string_view utf8, const Style& style, int maxWidth)
{
    int columns = 0;
    std::size_t i = 0;
    while (i < utf8.size()) {
        if (maxWidth >= 0 && columns >= maxWidth)
            break;
        std::size_t length = codePointLength(static_cast<unsigned char>(utf8[i]));
        if (i + length > utf8.size())
            length = utf8.size() - i;
        std::string_view glyph = utf8.substr(i, length);
        if (glyph == "\t" || glyph == "\n" || glyph == "\r")
            glyph = " ";
        put(x + columns, y, glyph, style);
        ++columns;
        i += length;
    }
    return columns;
}

void ScreenBuffer::field(int x, int y, int width, std::string_view utf8, const Style& style)
{
    const int used = text(x, y, utf8, style, width);
    for (int i = used; i < width; ++i)
        put(x + i, y, " ", style);
}

void ScreenBuffer::fill(const Rect& area, const Style& style, std::string_view glyph)
{
    for (int y = area.y; y < area.bottom(); ++y)
        for (int x = area.x; x < area.right(); ++x)
            put(x, y, glyph, style);
}

void ScreenBuffer::restyle(int x, int y, int width, const Style& style)
{
    for (int i = 0; i < width; ++i)
        if (inside(x + i, y))
            cellAt(x + i, y).style = style;
}

void ScreenBuffer::setBackground(int x, int y, int width, Color background)
{
    for (int i = 0; i < width; ++i)
        if (inside(x + i, y))
            cellAt(x + i, y).style.background = background;
}

void ScreenBuffer::horizontalLine(int x, int y, int width, const Style& style)
{
    for (int i = 0; i < width; ++i)
        put(x + i, y, activeBorders->horizontal, style);
}

void ScreenBuffer::box(const Rect& area, std::string_view title, const Style& borderStyle, const Style& titleStyle)
{
    if (area.width < 2 || area.height < 2)
        return;
    const BorderGlyphs& g = *activeBorders;
    const int right = area.right() - 1;
    const int bottom = area.bottom() - 1;
    put(area.x, area.y, g.topLeft, borderStyle);
    put(right, area.y, g.topRight, borderStyle);
    put(area.x, bottom, g.bottomLeft, borderStyle);
    put(right, bottom, g.bottomRight, borderStyle);
    horizontalLine(area.x + 1, area.y, area.width - 2, borderStyle);
    horizontalLine(area.x + 1, bottom, area.width - 2, borderStyle);
    for (int y = area.y + 1; y < bottom; ++y) {
        put(area.x, y, g.vertical, borderStyle);
        put(right, y, g.vertical, borderStyle);
    }
    if (!title.empty() && area.width > 6) {
        const std::string label = " " + std::string(title) + " ";
        text(area.x + 2, area.y, label, titleStyle, area.width - 4);
    }
}

} // namespace ovc::ui
