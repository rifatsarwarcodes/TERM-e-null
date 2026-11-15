#include "dashboard/TextScreen.hpp"

#include "ui/Theme.hpp"

#include <algorithm>

namespace ovc::dashboard {

using platform::KeyCode;
namespace theme = ui::theme;

TextScreen::TextScreen(std::string title, std::vector<std::string> lines)
    : title_(std::move(title))
    , lines_(std::move(lines))
{
    if (lines_.empty())
        lines_.push_back("(empty)");
}

void TextScreen::draw(ui::ScreenBuffer& screen)
{
    chrome::header(screen, "VIEW", title_);
    const ui::Rect box{0, 1, screen.width(), screen.height() - 2};
    screen.box(box, std::to_string(lines_.size()) + " lines", theme::border(), theme::title());
    const ui::Rect inner = box.inner();
    rows_ = std::max(1, inner.height);
    for (int row = 0; row < inner.height; ++row) {
        const std::size_t index = top_ + static_cast<std::size_t>(row);
        if (index >= lines_.size())
            break;
        const std::string number = std::to_string(index + 1);
        screen.text(inner.x + 4 - static_cast<int>(number.size()), inner.y + row, number, theme::lineNumber());
        screen.text(inner.x + 6, inner.y + row, lines_[index], theme::text(), inner.width - 7);
    }
    chrome::keyBar(screen, "Up/Down PgUp/PgDn scroll   Home/End   Esc back   Q quit");
}

ScreenResult TextScreen::handleKey(const platform::KeyEvent& key)
{
    const std::size_t page = static_cast<std::size_t>(rows_);
    const std::size_t last = lines_.size() > page ? lines_.size() - page : 0;
    if (key.is(KeyCode::Escape)) return ScreenResult::back();
    if (key.isChar('q') || key.isChar('Q')) return ScreenResult::quit();
    if (key.is(KeyCode::Up) && top_ > 0) --top_;
    else if (key.is(KeyCode::Down) && top_ < last) ++top_;
    else if (key.is(KeyCode::PageUp)) top_ = top_ > page ? top_ - page : 0;
    else if (key.is(KeyCode::PageDown)) top_ = std::min(last, top_ + page);
    else if (key.is(KeyCode::Home)) top_ = 0;
    else if (key.is(KeyCode::End)) top_ = last;
    return ScreenResult::stay();
}

} // namespace ovc::dashboard
