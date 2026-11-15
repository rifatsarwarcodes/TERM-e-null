#include "dashboard/SessionScreen.hpp"

#include "common/Files.hpp"
#include "common/Strings.hpp"
#include "common/Time.hpp"
#include "dashboard/EventText.hpp"
#include "dashboard/OriginViewScreen.hpp"
#include "dashboard/ReplayScreen.hpp"
#include "dashboard/ReportWriter.hpp"
#include "dashboard/TextScreen.hpp"
#include "evidence/Seal.hpp"
#include "ui/Theme.hpp"

#include <algorithm>

namespace ovc::dashboard {

using platform::KeyCode;
using session::Event;
using session::EventType;
namespace theme = ui::theme;

namespace {

ui::Style levelChip(penalty::Level level)
{
    switch (level) {
    case penalty::Level::Ok: return theme::okChip();
    case penalty::Level::Warning: return theme::warnChip();
    case penalty::Level::Flagged: return theme::badChip();
    }
    return theme::okChip();
}

std::string elapsedLabel(const SessionRecord& record, timeutil::Millis time)
{
    return timeutil::formatDuration(time - record.info.startedAt);
}

void label(ui::ScreenBuffer& screen, int x, int y, int width, const std::string& name, const std::string& value,
    const ui::Style& valueStyle = theme::text())
{
    screen.text(x, y, name, theme::dim(), 14);
    screen.text(x + 14, y, value, valueStyle, width - 14);
}

} // namespace

SessionScreen::SessionScreen(std::shared_ptr<const SessionRecord> record)
    : record_(std::move(record))
{
}

void SessionScreen::draw(ui::ScreenBuffer& screen)
{
    const SessionRecord& r = *record_;
    const std::string header = r.info.studentName + "  " + r.info.studentId + "   " + r.info.examCode + "   "
        + timeutil::formatDateTime(r.info.startedAt) + " (" + timeutil::formatSpan(r.duration) + ")";
    chrome::header(screen, "SESSION", header,
        std::string(penalty::levelName(r.level)) + " " + std::to_string(r.totalPoints) + " pts", levelChip(r.level));
    drawIntegrity(screen);

    const int top = 2;
    const int bottom = screen.height() - 1;
    const int height = bottom - top;
    const int leftWidth = std::max(40, screen.width() * 3 / 5);
    const int rightWidth = screen.width() - leftWidth;
    const int timelineHeight = std::max(6, height * 11 / 20);

    drawTimeline(screen, {0, top, leftWidth, timelineHeight});
    drawHistory(screen, {0, top + timelineHeight, leftWidth, height - timelineHeight});
    const int detailHeight = std::max(6, height * 2 / 5);
    const int penaltyHeight = std::max(5, height / 3);
    drawDetail(screen, {leftWidth, top, rightWidth, detailHeight});
    drawPenalty(screen, {leftWidth, top + detailHeight, rightWidth, penaltyHeight});
    drawStatus(screen, {leftWidth, top + detailHeight + penaltyHeight, rightWidth, height - detailHeight - penaltyHeight});

    if (typingReceipt_) {
        const int y = screen.height() - 1;
        screen.fill({0, y, screen.width(), 1}, theme::keyBar());
        const int x = screen.text(1, y, "Receipt code from the attendance sheet: ", theme::keyBar());
        const int end = x + 1 + screen.text(x + 1, y, receiptInput_, theme::keyBar().withBold());
        screen.setCursor(end, y);
    } else {
        chrome::keyBar(screen, "Up/Down event  Enter open  G origin  V viva focus  R replay  O outputs  K receipt  X export  Esc back  Q quit", message_);
    }
}

void SessionScreen::drawIntegrity(ui::ScreenBuffer& screen)
{
    const IntegrityReport& i = record_->integrity;
    screen.fill({0, 1, screen.width(), 1}, theme::subBar());
    int x = 1;
    auto item = [&](const std::string& name, const std::string& value, bool good) {
        x += screen.text(x, 1, name + " ", theme::subBar());
        const ui::Style style = theme::subBar().withForeground(good ? theme::ok().foreground : theme::bad().foreground).withBold();
        x += screen.text(x, 1, value, style) + 3;
    };
    item("Log:", i.eventsIntact ? "INTACT" : "BROKEN #" + std::to_string(i.eventsBrokenAt.value_or(0)), i.eventsIntact);
    item("Journal:", i.journalIntact && i.journalConsistent ? "INTACT" : "BROKEN", i.journalIntact && i.journalConsistent);
    item("Status:", i.status(), i.status() == "SEALED");
    item("Receipt", i.receipt + (receiptCheck_.empty() ? " (K to check)" : " " + receiptCheck_),
        receiptCheck_.empty() || receiptCheck_ == "MATCHES SHEET");
    if (i.submissionMatches)
        item("File:", *i.submissionMatches ? "MATCHES" : "CHANGED", *i.submissionMatches);
    if (i.packageMatches)
        item("Package:", *i.packageMatches ? "MATCHES" : "DIFFERENT", *i.packageMatches);
}

void SessionScreen::drawTimeline(ui::ScreenBuffer& screen, const ui::Rect& area)
{
    screen.box(area, "1 EVENT TIMELINE", theme::border(), theme::title());
    const ui::Rect inner = area.inner();
    const auto& events = record_->events;
    timelineRows_ = std::max(1, inner.height - 1);
    if (selected_ < top_)
        top_ = selected_;
    if (selected_ >= top_ + static_cast<std::size_t>(timelineRows_))
        top_ = selected_ - static_cast<std::size_t>(timelineRows_) + 1;

    for (int row = 0; row < timelineRows_; ++row) {
        const std::size_t index = top_ + static_cast<std::size_t>(row);
        if (index >= events.size())
            break;
        const Event& event = events[index];
        const int y = inner.y + row;
        char number[16];
        std::snprintf(number, sizeof number, "#%03llu", static_cast<unsigned long long>(event.sequence()));
        screen.text(inner.x + 1, y, std::string(number) + " " + elapsedLabel(*record_, event.time()), theme::dim());
        screen.text(inner.x + 13, y, session::eventTypeName(event.type()), eventStyle(event), 20);
        screen.text(inner.x + 34, y, summariseEvent(event), theme::text(), inner.width - 35);
        if (index == selected_)
            screen.restyle(inner.x, y, inner.width, theme::selection());
    }
    screen.text(inner.x + 1, inner.bottom() - 1,
        "Event " + std::to_string(events.empty() ? 0 : selected_ + 1) + " of " + std::to_string(events.size()),
        theme::dim());
}

void SessionScreen::drawHistory(ui::ScreenBuffer& screen, const ui::Rect& area)
{
    screen.box(area, "2 COMPILE, RUN AND TEST HISTORY", theme::border(), theme::title());
    const ui::Rect inner = area.inner();
    screen.text(inner.x + 1, inner.y, "#      Time    Action   Result", theme::heading());
    int row = 1;
    for (const auto& event : record_->events) {
        if (event.type() != EventType::Compile && event.type() != EventType::Run && event.type() != EventType::Test)
            continue;
        if (row >= inner.height)
            break;
        char number[16];
        std::snprintf(number, sizeof number, "#%03llu", static_cast<unsigned long long>(event.sequence()));
        const int y = inner.y + row++;
        screen.text(inner.x + 1, y, std::string(number) + "  " + elapsedLabel(*record_, event.time()), theme::text());
        screen.text(inner.x + 16, y, session::eventTypeName(event.type()), theme::text());
        screen.text(inner.x + 25, y, summariseEvent(event), eventStyle(event), inner.width - 26);
    }
    if (row == 1)
        screen.text(inner.x + 1, inner.y + 1, "Nothing was compiled.", theme::dim());
}

void SessionScreen::drawDetail(ui::ScreenBuffer& screen, const ui::Rect& area)
{
    const ui::Rect inner = area.inner();
    if (record_->events.empty()) {
        screen.box(area, "3 DETAIL", theme::border(), theme::title());
        return;
    }
    const Event& event = record_->events[selected_];
    const int x = inner.x + 1;
    const int w = inner.width - 2;
    int y = inner.y;

    if (event.type() == EventType::Paste) {
        screen.box(area, "3 PASTE DETAIL  #" + std::to_string(event.sequence()), theme::border(), theme::title());
        const PasteInfo* paste = record_->paste(event.sequence());
        const bool outside = event.getOr("src") == "outside";
        label(screen, x, y++, w, "Source", event.getOr("src"), outside ? theme::bad() : theme::ok());
        label(screen, x, y++, w, "Detected by", event.getOr("conf") == "confirmed" ? "bracketed paste" : event.getOr("conf") == "certain" ? "editor p/P" : "input burst");
        label(screen, x, y++, w, "Size", event.getOr("chars") + " chars / " + event.getOr("lines") + " lines");
        if (paste != nullptr) {
            label(screen, x, y++, w, "Kept", percentText(paste->retained) + " unchanged at the end",
                paste->retained >= record_->policy.unchangedRatio && outside ? theme::bad() : theme::text());
            if (paste->sinceFocusReturn)
                label(screen, x, y++, w, "After return", timeutil::formatSpan(*paste->sinceFocusReturn) + " after coming back", theme::warn());
            label(screen, x, y++, w, "Note", paste->note.empty() ? "none" : "\"" + paste->note + "\"");
        }
        label(screen, x, y + 1, w, "Content", "Enter shows " + event.getOr("file"), theme::dim());
        return;
    }

    screen.box(area, "3 EVENT DETAIL  #" + std::to_string(event.sequence()), theme::border(), theme::title());
    label(screen, x, y++, w, "Type", session::eventTypeName(event.type()), eventStyle(event));
    label(screen, x, y++, w, "Time", timeutil::formatClock(event.time()));
    for (const auto& [key, value] : event.fields()) {
        if (y >= inner.bottom() - 1)
            break;
        label(screen, x, y++, w, key, value.size() > 64 ? value.substr(0, 16) + "..." : value);
    }
    if (event.has("file"))
        screen.text(x, inner.bottom() - 1, "Enter shows the saved output", theme::dim());
}

void SessionScreen::drawPenalty(ui::ScreenBuffer& screen, const ui::Rect& area)
{
    screen.box(area, "4 PENALTY (recalculated)", theme::border(), theme::title());
    const ui::Rect inner = area.inner();
    int y = inner.y;
    for (const auto& [rule, points] : record_->pointsByRule) {
        if (y >= inner.bottom() - 2)
            break;
        screen.text(inner.x + 1, y, rule, theme::text(), inner.width - 8);
        screen.text(inner.right() - 5, y++, (points > 0 ? "+" : " ") + std::to_string(points), points > 0 ? theme::warn() : theme::dim());
    }
    screen.text(inner.x + 1, inner.bottom() - 2, "Total", theme::heading());
    screen.text(inner.right() - 5, inner.bottom() - 2, std::to_string(record_->totalPoints), theme::heading());
    screen.text(inner.x + 1, inner.bottom() - 1,
        "Warning >= " + std::to_string(record_->policy.warningAt) + "   Flagged >= " + std::to_string(record_->policy.flaggedAt),
        theme::dim());
}

void SessionScreen::drawStatus(ui::ScreenBuffer& screen, const ui::Rect& area)
{
    screen.box(area, "5 STATUS", theme::border(), theme::title());
    const ui::Rect inner = area.inner();
    const SessionRecord& r = *record_;
    int y = inner.y;
    const int x = inner.x + 1;
    screen.text(x, y, "Level: ", theme::text());
    screen.text(x + 7, y++, penalty::levelName(r.level), r.level == penalty::Level::Ok ? theme::ok() : r.level == penalty::Level::Warning ? theme::warn() : theme::bad());
    const OriginShares& s = r.shares;
    screen.text(x, y++, "Typed " + percentText(s.share(s.typed)) + ", outside paste " + percentText(s.share(s.outside)), theme::text(), inner.width - 2);
    screen.text(x, y++, "Viva focus list: " + std::to_string(r.focus.size()) + " item(s)  (V)", r.focus.empty() ? theme::dim() : theme::warn(), inner.width - 2);
    if (r.integrity.pointsMatchLog && !*r.integrity.pointsMatchLog)
        screen.text(x, y++, "Logged points differ from recalculation.", theme::bad(), inner.width - 2);
    if (y < inner.bottom())
        screen.text(x, inner.bottom() - 1, "Deductions are decided by the examiner.", theme::dim(), inner.width - 2);
}

ScreenResult SessionScreen::openSelected()
{
    if (record_->events.empty())
        return ScreenResult::stay();
    const Event& event = record_->events[selected_];
    const std::string file = event.getOr("file");
    if (file.empty()) {
        message_ = "This event has no saved content.";
        return ScreenResult::stay();
    }
    std::string title = std::string(session::eventTypeName(event.type())) + " #" + std::to_string(event.sequence()) + "  (" + file + ")";
    return ScreenResult::push(std::make_unique<TextScreen>(title, strings::splitLines(record_->readFile(file))));
}

ScreenResult SessionScreen::openOutputs()
{
    std::vector<std::string> lines;
    for (const auto& event : record_->events) {
        if (event.type() != EventType::Compile && event.type() != EventType::Run && event.type() != EventType::Test)
            continue;
        lines.push_back("==== #" + std::to_string(event.sequence()) + " " + session::eventTypeName(event.type()) + " at "
            + timeutil::formatClock(event.time()) + ": " + summariseEvent(event));
        for (const auto& line : strings::splitLines(record_->readFile(event.getOr("file"))))
            lines.push_back("  " + line);
        lines.emplace_back();
    }
    return ScreenResult::push(std::make_unique<TextScreen>("All compile, run and test outputs", lines));
}

void SessionScreen::exportReport()
{
    const std::string name = "report_" + record_->info.studentId + ".txt";
    if (files::writeAtomically(name, ReportWriter::sessionReport(*record_)))
        message_ = "Report written to " + name;
    else
        message_ = "Could not write " + name;
}

void SessionScreen::checkReceipt(const std::string& typed)
{
    if (strings::trim(typed).empty())
        return;
    receiptCheck_ = evidence::receiptsMatch(typed, record_->integrity.receipt) ? "MATCHES SHEET" : "DOES NOT MATCH SHEET";
}

ScreenResult SessionScreen::handleKey(const platform::KeyEvent& key)
{
    if (typingReceipt_) {
        if (key.is(KeyCode::Enter)) {
            typingReceipt_ = false;
            checkReceipt(receiptInput_);
        } else if (key.is(KeyCode::Escape)) {
            typingReceipt_ = false;
        } else if (key.is(KeyCode::Backspace)) {
            strings::popCodePoint(receiptInput_);
        } else if (key.isPrintable()) {
            receiptInput_ += key.text;
        }
        return ScreenResult::stay();
    }

    message_.clear();
    const std::size_t count = record_->events.size();
    const std::size_t page = static_cast<std::size_t>(timelineRows_);
    if (key.is(KeyCode::Up) && selected_ > 0) --selected_;
    else if (key.is(KeyCode::Down) && selected_ + 1 < count) ++selected_;
    else if (key.is(KeyCode::PageUp)) selected_ = selected_ > page ? selected_ - page : 0;
    else if (key.is(KeyCode::PageDown) && count > 0) selected_ = std::min(count - 1, selected_ + page);
    else if (key.is(KeyCode::Home)) selected_ = 0;
    else if (key.is(KeyCode::End) && count > 0) selected_ = count - 1;
    else if (key.is(KeyCode::Enter)) return openSelected();
    else if (key.is(KeyCode::Escape)) return ScreenResult::back();
    else if (key.isPrintable()) {
        switch (key.text[0]) {
        case 'q': case 'Q': return ScreenResult::quit();
        case 'g': case 'G': return ScreenResult::push(std::make_unique<OriginViewScreen>(record_, false));
        case 'v': case 'V': return ScreenResult::push(std::make_unique<OriginViewScreen>(record_, true));
        case 'r': case 'R': return ScreenResult::push(std::make_unique<ReplayScreen>(record_));
        case 'o': case 'O': return openOutputs();
        case 'x': case 'X': exportReport(); break;
        case 'k': case 'K':
            typingReceipt_ = true;
            receiptInput_.clear();
            break;
        default: break;
        }
    }
    return ScreenResult::stay();
}

} // namespace ovc::dashboard
