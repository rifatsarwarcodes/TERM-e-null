#include "dashboard/ReplayScreen.hpp"

#include "common/Time.hpp"
#include "dashboard/EventText.hpp"
#include "ui/Theme.hpp"

#include <algorithm>

namespace ovc::dashboard {

using platform::KeyCode;
using session::Event;
using session::EventType;
namespace theme = ui::theme;

namespace {

bool isMilestone(const Event& event)
{
    switch (event.type()) {
    case EventType::Paste:
    case EventType::Compile:
    case EventType::Test:
    case EventType::Run:
    case EventType::FocusGained:
    case EventType::SessionResumed:
        return true;
    default:
        return false;
    }
}

} // namespace

ReplayScreen::ReplayScreen(std::shared_ptr<const SessionRecord> record)
    : record_(std::move(record))
{
    moveTo(0);
}

void ReplayScreen::moveTo(std::size_t position)
{
    position_ = std::min(position, record_->journal.size());
    state_ = evidence::JournalReplayer::replay(record_->journal, position_);
}

timeutil::Millis ReplayScreen::currentTime() const
{
    if (position_ == 0)
        return record_->info.startedAt;
    return record_->journal[position_ - 1].time;
}

void ReplayScreen::jumpToEvent(bool forward)
{
    const timeutil::Millis now = currentTime();
    const auto& journal = record_->journal;
    if (forward) {
        for (const auto& event : record_->events) {
            if (!isMilestone(event) || event.time() <= now)
                continue;
            // Apply every edit made up to and including this event.
            std::size_t target = position_;
            while (target < journal.size() && journal[target].time <= event.time())
                ++target;
            moveTo(std::max(target, position_ + 1));
            return;
        }
        moveTo(journal.size());
    } else {
        for (auto it = record_->events.rbegin(); it != record_->events.rend(); ++it) {
            if (!isMilestone(*it) || it->time() >= now)
                continue;
            std::size_t target = 0;
            while (target < journal.size() && journal[target].time <= it->time())
                ++target;
            moveTo(std::min(target, position_ > 0 ? position_ - 1 : 0));
            return;
        }
        moveTo(0);
    }
}

void ReplayScreen::draw(ui::ScreenBuffer& screen)
{
    const SessionRecord& r = *record_;
    const timeutil::Millis now = currentTime();
    chrome::header(screen, "REPLAY", r.info.studentName + "  " + r.info.studentId + "   edit "
            + std::to_string(position_) + " of " + std::to_string(r.journal.size()) + "   at "
            + timeutil::formatClock(now) + " (+" + timeutil::formatDuration(now - r.info.startedAt) + ")");

    const int sideWidth = std::clamp(screen.width() / 3, 32, 48);
    const ui::Rect codeBox{0, 1, screen.width() - sideWidth, screen.height() - 2};
    screen.box(codeBox, r.info.fileName, theme::border(), theme::title());
    std::optional<std::size_t> cursor;
    if (position_ > 0) {
        const auto& last = r.journal[position_ - 1];
        cursor = std::min(last.position + (last.kind == evidence::JournalEntry::Kind::Insert ? last.text.size() : 0),
            state_.buffer.size());
    }
    ui::EditorViewOptions options;
    options.showOrigins = true;
    options.syntaxHighlight = false;
    view_.drawBuffer(screen, codeBox.inner(), state_.buffer, cursor.value_or(0), false, options);

    // Events up to this moment, newest first.
    const ui::Rect side{codeBox.right(), 1, sideWidth, screen.height() - 2};
    screen.box(side, "EVENTS SO FAR", theme::border(), theme::title());
    const ui::Rect inner = side.inner();
    int y = inner.y;
    for (auto it = r.events.rbegin(); it != r.events.rend() && y < inner.bottom(); ++it) {
        if (it->time() > now)
            continue;
        screen.text(inner.x + 1, y, timeutil::formatDuration(it->time() - r.info.startedAt), theme::dim());
        screen.text(inner.x + 8, y++, session::eventTypeName(it->type()), eventStyle(*it), inner.width - 9);
        if (y < inner.bottom() && isMilestone(*it))
            screen.text(inner.x + 10, y++, summariseEvent(*it), theme::text(), inner.width - 11);
    }
    chrome::keyBar(screen, "Left/Right one edit  Up/Down 10  PgUp/PgDn 100  n/N next/previous moment  Home/End  Esc back");
}

ScreenResult ReplayScreen::handleKey(const platform::KeyEvent& key)
{
    if (key.is(KeyCode::Escape)) return ScreenResult::back();
    if (key.isChar('q') || key.isChar('Q')) return ScreenResult::quit();
    auto step = [this](long delta) {
        const long target = static_cast<long>(position_) + delta;
        moveTo(static_cast<std::size_t>(std::max(0L, target)));
    };
    if (key.is(KeyCode::Right)) step(1);
    else if (key.is(KeyCode::Left)) step(-1);
    else if (key.is(KeyCode::Down)) step(10);
    else if (key.is(KeyCode::Up)) step(-10);
    else if (key.is(KeyCode::PageDown)) step(100);
    else if (key.is(KeyCode::PageUp)) step(-100);
    else if (key.is(KeyCode::Home)) moveTo(0);
    else if (key.is(KeyCode::End)) moveTo(record_->journal.size());
    else if (key.isChar('n')) jumpToEvent(true);
    else if (key.isChar('N')) jumpToEvent(false);
    return ScreenResult::stay();
}

} // namespace ovc::dashboard
