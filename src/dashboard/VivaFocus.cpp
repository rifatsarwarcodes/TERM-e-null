#include "dashboard/VivaFocus.hpp"

#include "common/Time.hpp"

#include <algorithm>
#include <cstdio>

namespace ovc::dashboard {

using session::Event;
using session::EventType;

namespace {

std::string percent(double ratio)
{
    char text[8];
    std::snprintf(text, sizeof text, "%d%%", static_cast<int>(ratio * 100 + 0.5));
    return text;
}

// Line ranges (1-based) covered by the characters whose index passes `take`.
template <typename Predicate>
std::vector<std::pair<std::size_t, std::size_t>> lineRanges(const editor::TextBuffer& buffer, Predicate take)
{
    std::vector<std::pair<std::size_t, std::size_t>> ranges;
    const std::size_t size = buffer.size();
    std::size_t i = 0;
    while (i < size) {
        if (!take(i) || buffer.at(i) == '\n') {
            ++i;
            continue;
        }
        const std::size_t first = buffer.positionOf(i).line + 1;
        std::size_t end = i;
        while (end < size && (take(end) || buffer.at(end) == '\n'))
            ++end;
        std::size_t last = buffer.positionOf(end > 0 ? end - 1 : 0).line + 1;
        if (end > 0 && buffer.at(end - 1) == '\n' && last > first)
            --last;
        ranges.emplace_back(first, last);
        i = end;
    }
    return ranges;
}

} // namespace

std::vector<FocusItem> mergeRanges(std::vector<FocusItem> items)
{
    std::sort(items.begin(), items.end(), [](const FocusItem& a, const FocusItem& b) {
        if (a.reason != b.reason)
            return a.reason < b.reason;
        return a.firstLine < b.firstLine;
    });
    std::vector<FocusItem> merged;
    for (const auto& item : items) {
        if (!merged.empty() && merged.back().reason == item.reason && item.firstLine <= merged.back().lastLine + 1) {
            merged.back().lastLine = std::max(merged.back().lastLine, item.lastLine);
            merged.back().priority = std::max(merged.back().priority, item.priority);
        } else {
            merged.push_back(item);
        }
    }
    std::sort(merged.begin(), merged.end(), [](const FocusItem& a, const FocusItem& b) {
        if (a.priority != b.priority)
            return a.priority > b.priority;
        return a.firstLine < b.firstLine;
    });
    return merged;
}

std::vector<FocusItem> VivaFocusBuilder::build(const SessionRecord& record)
{
    const editor::TextBuffer& buffer = record.replay.buffer;
    const auto& times = record.replay.insertedAt;
    std::vector<FocusItem> items;

    // 1. Outside pastes still present in the final code.
    for (const auto& paste : record.pastes) {
        if (paste.source != "outside" || paste.retained <= 0.0)
            continue;
        const editor::Origin origin = editor::Origin::paste(editor::OriginKind::Outside, static_cast<int>(paste.sequence));
        std::string reason = "Outside paste #" + std::to_string(paste.sequence) + ", " + percent(paste.retained) + " kept";
        if (paste.sinceFocusReturn && *paste.sinceFocusReturn <= record.policy.focusReturnSeconds * 1000LL)
            reason += ", pasted " + timeutil::formatSpan(*paste.sinceFocusReturn) + " after returning to the window";
        const int priority = 50 + static_cast<int>(paste.retained * 40) + (paste.chars > record.policy.largePasteChars ? 10 : 0);
        for (const auto& range : lineRanges(buffer, [&](std::size_t i) { return buffer.origins().at(i) == origin; }))
            items.push_back({range.first, range.second, reason, priority});
    }

    // 2. Code written soon after coming back to the window.
    const timeutil::Millis window = record.policy.focusReturnSeconds * 1000LL;
    for (const auto& event : record.events) {
        if (event.type() != EventType::FocusGained || event.getInt("away_ms") < 10'000)
            continue;
        const timeutil::Millis from = event.time();
        const std::string reason = "Written within " + std::to_string(record.policy.focusReturnSeconds)
            + " s of returning (away " + timeutil::formatSpan(event.getInt("away_ms")) + ")";
        for (const auto& range : lineRanges(buffer, [&](std::size_t i) {
                 return i < times.size() && times[i] >= from && times[i] <= from + window;
             }))
            items.push_back({range.first, range.second, reason, 40});
    }

    // 3. Code changed between a failing test and the next passing one.
    const Event* lastFailing = nullptr;
    for (const auto& event : record.events) {
        if (event.type() != EventType::Test)
            continue;
        const bool passed = event.getInt("passed") == event.getInt("total") && event.getInt("total") > 0;
        if (!passed) {
            lastFailing = &event;
            continue;
        }
        if (lastFailing == nullptr)
            continue;
        const timeutil::Millis from = lastFailing->time();
        const timeutil::Millis to = event.time();
        const std::string reason = "Changed between failing test #" + std::to_string(lastFailing->sequence())
            + " and passing test #" + std::to_string(event.sequence());
        for (const auto& range : lineRanges(buffer, [&](std::size_t i) {
                 return i < times.size() && times[i] > from && times[i] <= to;
             }))
            items.push_back({range.first, range.second, reason, 30});
        lastFailing = nullptr;
    }
    return mergeRanges(items);
}

} // namespace ovc::dashboard
