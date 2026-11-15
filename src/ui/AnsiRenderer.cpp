#include "ui/AnsiRenderer.hpp"

namespace ovc::ui {

bool AnsiRenderer::rowChanged(const ScreenBuffer& frame, int y) const
{
    for (int x = 0; x < frame.width(); ++x)
        if (frame.at(x, y) != previous_.at(x, y))
            return true;
    return false;
}

void AnsiRenderer::appendStyle(std::string& out, const Style& style)
{
    out += "\x1b[0";
    if (style.bold)
        out += ";1";
    if (style.underline)
        out += ";4";
    if (style.reverse)
        out += ";7";
    if (style.foreground.index >= 0)
        out += ";38;5;" + std::to_string(style.foreground.index);
    if (style.background.index >= 0)
        out += ";48;5;" + std::to_string(style.background.index);
    out += "m";
}

std::string AnsiRenderer::render(const ScreenBuffer& frame)
{
    if (!valid_ || previous_.width() != frame.width() || previous_.height() != frame.height())
        valid_ = false;

    std::string out = "\x1b[?25l"; // hide the cursor while drawing
    if (!valid_)
        out += "\x1b[0m\x1b[2J";

    for (int y = 0; y < frame.height(); ++y) {
        if (valid_ && !rowChanged(frame, y))
            continue;
        out += "\x1b[" + std::to_string(y + 1) + ";1H";
        const Style* current = nullptr;
        for (int x = 0; x < frame.width(); ++x) {
            const Cell& cell = frame.at(x, y);
            if (current == nullptr || *current != cell.style) {
                appendStyle(out, cell.style);
                current = &cell.style;
            }
            out += cell.glyph;
        }
        out += "\x1b[0m";
    }

    if (frame.cursorVisible()) {
        out += "\x1b[" + std::to_string(frame.cursorY() + 1) + ";" + std::to_string(frame.cursorX() + 1) + "H";
        out += "\x1b[?25h";
    }

    previous_ = frame;
    valid_ = true;
    return out;
}

} // namespace ovc::ui
