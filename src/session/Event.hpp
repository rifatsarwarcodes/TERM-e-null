#pragma once

#include "common/Time.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ovc::session {

enum class EventType {
    SessionStart,
    SessionResumed,
    SessionEnd,
    Save,
    Paste,
    PasteNote,
    FocusLost,
    FocusGained,
    Compile,
    Run,
    Test,
    FileChangedOutside,
    TimeWarning,
    RuleHit,
    LevelChange,
    DashboardOpened,
    DashboardClosed,
    PasscodeFailed,
    Unknown,
};

const char* eventTypeName(EventType type);
EventType eventTypeFromName(std::string_view name);

// One recorded action inside a session. The details are an ordered list of
// key/value pairs so every event type can be written in the same format.
class Event {
public:
    using Field = std::pair<std::string, std::string>;

    Event() = default;
    Event(EventType type, std::vector<Field> fields = {});

    EventType type() const { return type_; }
    std::uint64_t sequence() const { return sequence_; }
    timeutil::Millis time() const { return time_; }
    const std::vector<Field>& fields() const { return fields_; }

    // Set by the session when the event is published.
    void stamp(std::uint64_t sequence, timeutil::Millis time);

    Event& set(const std::string& key, std::string value);
    Event& set(const std::string& key, long long value);
    Event& set(const std::string& key, double value);

    std::optional<std::string> get(const std::string& key) const;
    std::string getOr(const std::string& key, const std::string& fallback = "") const;
    long long getInt(const std::string& key, long long fallback = 0) const;
    double getDouble(const std::string& key, double fallback = 0.0) const;
    bool has(const std::string& key) const { return get(key).has_value(); }

    // "seq|epochMillis|TYPE|key=value;key=value" (fields escaped).
    std::string encode() const;
    static std::optional<Event> decode(std::string_view body);

private:
    EventType type_ = EventType::Unknown;
    std::uint64_t sequence_ = 0;
    timeutil::Millis time_ = 0;
    std::vector<Field> fields_;
};

} // namespace ovc::session
