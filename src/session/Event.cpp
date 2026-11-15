#include "session/Event.hpp"

#include "common/Escape.hpp"
#include "common/Strings.hpp"

#include <cstdio>

namespace ovc::session {

namespace {

struct TypeName {
    EventType type;
    const char* name;
};

constexpr TypeName TypeNames[] = {
    {EventType::SessionStart, "SESSION_START"},
    {EventType::SessionResumed, "SESSION_RESUMED"},
    {EventType::SessionEnd, "SESSION_END"},
    {EventType::Save, "SAVE"},
    {EventType::Paste, "PASTE"},
    {EventType::PasteNote, "PASTE_NOTE"},
    {EventType::FocusLost, "FOCUS_LOST"},
    {EventType::FocusGained, "FOCUS_GAINED"},
    {EventType::Compile, "COMPILE"},
    {EventType::Run, "RUN"},
    {EventType::Test, "TEST"},
    {EventType::FileChangedOutside, "FILE_CHANGED_OUTSIDE"},
    {EventType::TimeWarning, "TIME_WARNING"},
    {EventType::RuleHit, "RULE_HIT"},
    {EventType::LevelChange, "LEVEL_CHANGE"},
    {EventType::DashboardOpened, "DASHBOARD_OPENED"},
    {EventType::DashboardClosed, "DASHBOARD_CLOSED"},
    {EventType::PasscodeFailed, "PASSCODE_FAILED"},
};

} // namespace

const char* eventTypeName(EventType type)
{
    for (const auto& entry : TypeNames)
        if (entry.type == type)
            return entry.name;
    return "UNKNOWN";
}

EventType eventTypeFromName(std::string_view name)
{
    for (const auto& entry : TypeNames)
        if (name == entry.name)
            return entry.type;
    return EventType::Unknown;
}

Event::Event(EventType type, std::vector<Field> fields)
    : type_(type)
    , fields_(std::move(fields))
{
}

void Event::stamp(std::uint64_t sequence, timeutil::Millis time)
{
    sequence_ = sequence;
    time_ = time;
}

Event& Event::set(const std::string& key, std::string value)
{
    for (auto& field : fields_) {
        if (field.first == key) {
            field.second = std::move(value);
            return *this;
        }
    }
    fields_.emplace_back(key, std::move(value));
    return *this;
}

Event& Event::set(const std::string& key, long long value)
{
    return set(key, std::to_string(value));
}

Event& Event::set(const std::string& key, double value)
{
    char text[32];
    std::snprintf(text, sizeof text, "%.2f", value);
    return set(key, std::string(text));
}

std::optional<std::string> Event::get(const std::string& key) const
{
    for (const auto& field : fields_)
        if (field.first == key)
            return field.second;
    return std::nullopt;
}

std::string Event::getOr(const std::string& key, const std::string& fallback) const
{
    return get(key).value_or(fallback);
}

long long Event::getInt(const std::string& key, long long fallback) const
{
    long long value = 0;
    const auto text = get(key);
    return text && strings::parseInt(*text, value) ? value : fallback;
}

double Event::getDouble(const std::string& key, double fallback) const
{
    double value = 0;
    const auto text = get(key);
    return text && strings::parseDouble(*text, value) ? value : fallback;
}

std::string Event::encode() const
{
    std::string body = std::to_string(sequence_) + "|" + std::to_string(time_) + "|" + eventTypeName(type_) + "|";
    for (std::size_t i = 0; i < fields_.size(); ++i) {
        if (i > 0)
            body += ";";
        body += escape::encode(fields_[i].first) + "=" + escape::encode(fields_[i].second);
    }
    return body;
}

std::optional<Event> Event::decode(std::string_view body)
{
    const std::size_t first = escape::findUnescaped(body, '|');
    const std::size_t second = escape::findUnescaped(body, '|', first + 1);
    const std::size_t third = escape::findUnescaped(body, '|', second + 1);
    if (first == std::string_view::npos || second == std::string_view::npos || third == std::string_view::npos)
        return std::nullopt;

    std::uint64_t sequence = 0;
    long long time = 0;
    if (!strings::parseUnsigned(body.substr(0, first), sequence)
        || !strings::parseInt(body.substr(first + 1, second - first - 1), time))
        return std::nullopt;

    Event event(eventTypeFromName(body.substr(second + 1, third - second - 1)));
    event.stamp(sequence, time);

    std::string_view rest = body.substr(third + 1);
    while (!rest.empty()) {
        std::size_t end = escape::findUnescaped(rest, ';');
        const std::string_view pair = rest.substr(0, end);
        const std::size_t equals = escape::findUnescaped(pair, '=');
        if (equals != std::string_view::npos)
            event.fields_.emplace_back(escape::decode(pair.substr(0, equals)), escape::decode(pair.substr(equals + 1)));
        if (end == std::string_view::npos)
            break;
        rest.remove_prefix(end + 1);
    }
    return event;
}

} // namespace ovc::session
