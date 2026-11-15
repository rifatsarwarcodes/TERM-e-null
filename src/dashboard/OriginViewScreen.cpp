#include "dashboard/OriginViewScreen.hpp"

#include "dashboard/ReplayScreen.hpp"
#include "dashboard/ReportWriter.hpp"
#include "ui/TextWrap.hpp"
#include "ui/Theme.hpp"

#include <algorithm>

namespace ovc::dashboard {

using platform::KeyCode;
namespace theme = ui::theme;

OriginViewScreen::OriginViewScreen(std::shared_ptr<const SessionRecord> record, bool startInFocusList)
    : record_(std::move(record))
    , focusList_(startInFocusList && !record_->focus.empty())
{
    if (focusList_)
        jumpToSelected();
}

void OriginViewScreen::jumpToSelected()
{
    if (selected_ < record_->focus.size()) {
        const std::size_t first = record_->focus[selected_].firstLine;
        line_ = first > 0 ? first - 1 : 0;
    }
}

void OriginViewScreen::draw(ui::ScreenBuffer& screen)
{
    const SessionRecord& r = *record_;
    chrome::header(screen, "CODE ORIGIN", r.info.studentName + "  " + r.info.studentId + "   final " + r.info.fileName
        + " (rebuilt from the edit journal)");

    const int sideWidth = std::clamp(screen.width() / 3, 34, 50);
    const ui::Rect codeBox{0, 1, screen.width() - sideWidth, screen.height() - 2};
    screen.box(codeBox, r.info.fileName, theme::border(), theme::title());
    codeRows_ = std::max(1, codeBox.height - 2);

    const editor::TextBuffer& buffer = r.replay.buffer;
    line_ = std::min(line_, buffer.lineCount() - 1);
    ui::EditorViewOptions options;
    options.showOrigins = true;
    options.syntaxHighlight = false;
    view_.drawBuffer(screen, codeBox.inner(), buffer, buffer.lineStart(line_), false, options);

    // Origin summary.
    const ui::Rect summary{codeBox.right(), 1, sideWidth, 9};
    screen.box(summary, "ORIGIN SUMMARY", theme::border(), theme::title());
    const OriginShares& s = r.shares;
    struct Row { const char* name; std::size_t count; ui::Style style; };
    const Row rows[] = {
        {"Typed by student", s.typed, theme::originTyped()},
        {"Outside paste", s.outside, theme::originOutside()},
        {"Self / inside paste", s.self + s.inside, theme::originSelf()},
        {"Allowed snippet", s.allowed, theme::originAllowed()},
        {"In the file at start", s.loaded, theme::originLoaded()},
    };
    int y = summary.y + 1;
    for (const auto& row : rows) {
        screen.text(summary.x + 2, y, "▌", row.style);
        screen.text(summary.x + 4, y, row.name, row.style, sideWidth - 12);
        screen.text(summary.right() - 6, y++, percentText(s.share(row.count)), theme::text());
    }
    screen.text(summary.x + 2, y + 1, "Share of the final file", theme::dim());

    // Viva focus list.
    const ui::Rect focusBox{codeBox.right(), summary.bottom(), sideWidth, screen.height() - 1 - summary.bottom()};
    screen.box(focusBox, focusList_ ? "VIVA FOCUS LIST (Tab: code)" : "VIVA FOCUS LIST (Tab: select)", theme::border(), theme::title());
    const ui::Rect inner = focusBox.inner();
    y = inner.y;
    if (r.focus.empty())
        screen.text(inner.x + 1, y, "Nothing in particular.", theme::dim());
    for (std::size_t i = 0; i < r.focus.size() && y < inner.bottom(); ++i) {
        const FocusItem& item = r.focus[i];
        const bool current = focusList_ && i == selected_;
        const ui::Style head = current ? theme::selection() : theme::heading();
        screen.field(inner.x, y++, inner.width,
            " " + std::to_string(i + 1) + ". Lines " + std::to_string(item.firstLine) + "-" + std::to_string(item.lastLine), head);
        for (const auto& line : ui::wrapText(item.reason, inner.width - 4)) {
            if (y >= inner.bottom())
                break;
            screen.text(inner.x + 4, y++, line, item.priority >= 70 ? theme::bad() : theme::warn());
        }
    }
    chrome::keyBar(screen, "Up/Down move  PgUp/PgDn scroll  Tab switch list/code  R replay  Esc back  Q quit");
}

ScreenResult OriginViewScreen::handleKey(const platform::KeyEvent& key)
{
    const std::size_t lines = record_->replay.buffer.lineCount();
    const std::size_t page = static_cast<std::size_t>(codeRows_);
    if (key.is(KeyCode::Escape)) return ScreenResult::back();
    if (key.isChar('q') || key.isChar('Q')) return ScreenResult::quit();
    if (key.isChar('r') || key.isChar('R')) return ScreenResult::push(std::make_unique<ReplayScreen>(record_));
    if (key.is(KeyCode::Tab)) {
        focusList_ = !focusList_ && !record_->focus.empty();
        if (focusList_)
            jumpToSelected();
        return ScreenResult::stay();
    }
    if (focusList_) {
        if (key.is(KeyCode::Up) && selected_ > 0) --selected_;
        else if (key.is(KeyCode::Down) && selected_ + 1 < record_->focus.size()) ++selected_;
        jumpToSelected();
    } else {
        if (key.is(KeyCode::Up) && line_ > 0) --line_;
        else if (key.is(KeyCode::Down) && line_ + 1 < lines) ++line_;
    }
    if (key.is(KeyCode::PageUp)) line_ = line_ > page ? line_ - page : 0;
    else if (key.is(KeyCode::PageDown)) line_ = std::min(lines - 1, line_ + page);
    else if (key.is(KeyCode::Home)) line_ = 0;
    else if (key.is(KeyCode::End)) line_ = lines - 1;
    return ScreenResult::stay();
}

} // namespace ovc::dashboard
