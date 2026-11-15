#include "dashboard/EventText.hpp"

#include "common/Time.hpp"
#include "ui/Theme.hpp"

namespace ovc::dashboard {

using session::Event;
using session::EventType;
namespace theme = ui::theme;

std::string summariseEvent(const Event& event)
{
    switch (event.type()) {
    case EventType::SessionStart:
        return event.getOr("name") + ", " + event.getOr("file") + ", limit " + event.getOr("time_limit_min") + " min";
    case EventType::SessionResumed: return "after an interruption";
    case EventType::SessionEnd: return "reason: " + event.getOr("reason");
    case EventType::Save: return event.getOr("lines") + " lines";
    case EventType::Paste:
        return event.getOr("src") + " | " + event.getOr("chars") + " ch | " + event.getOr("lines") + " ln | "
            + event.getOr("conf");
    case EventType::PasteNote: return "#" + event.getOr("paste") + " \"" + event.getOr("text") + "\"";
    case EventType::FocusLost: return "left the editor window";
    case EventType::FocusGained: return "back after " + timeutil::formatSpan(event.getInt("away_ms"));
    case EventType::Compile:
        return event.getInt("ok") ? "ok" : event.getOr("errors") + " error(s)";
    case EventType::Run:
        return event.getOr("mode") + ", exit " + event.getOr("exit") + (event.getInt("timed_out") ? ", timed out" : "");
    case EventType::Test: return event.getOr("passed") + "/" + event.getOr("total") + " passed";
    case EventType::FileChangedOutside: return "file changed by another program (" + event.getOr("when") + ")";
    case EventType::TimeWarning: return event.getOr("minutes_left") + " min left";
    case EventType::RuleHit: return event.getOr("rule") + " +" + event.getOr("points");
    case EventType::LevelChange: return "level " + event.getOr("level");
    case EventType::DashboardOpened: return "examiner opened the dashboard";
    case EventType::DashboardClosed: return "examiner closed the dashboard";
    case EventType::PasscodeFailed: return "wrong passcode (attempt " + event.getOr("attempts") + ")";
    case EventType::Unknown: break;
    }
    return "";
}

ui::Style eventStyle(const Event& event)
{
    switch (event.type()) {
    case EventType::Paste:
        return event.getOr("src") == "outside" ? theme::bad() : theme::text();
    case EventType::FocusLost:
    case EventType::FocusGained:
    case EventType::RuleHit:
    case EventType::TimeWarning:
    case EventType::SessionResumed:
        return theme::warn();
    case EventType::LevelChange:
    case EventType::FileChangedOutside:
    case EventType::PasscodeFailed:
        return theme::bad();
    case EventType::Compile:
        return event.getInt("ok") ? theme::ok() : theme::bad();
    case EventType::Test:
        return event.getInt("passed") == event.getInt("total") ? theme::ok() : theme::bad();
    default:
        return theme::text();
    }
}

} // namespace ovc::dashboard
