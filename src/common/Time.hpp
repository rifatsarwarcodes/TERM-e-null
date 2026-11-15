#pragma once

#include <cstdint>
#include <string>

namespace ovc::timeutil {

using Millis = std::int64_t;

// Wall-clock time in milliseconds since the Unix epoch.
Millis nowEpochMillis();

// Monotonic time in milliseconds; only differences are meaningful.
Millis nowSteadyMillis();

// "14:05:09" in local time.
std::string formatClock(Millis epochMillis);

// "2026-11-16 14:05:09" in local time.
std::string formatDateTime(Millis epochMillis);

// "1:05:09", "05:09" style remaining-time text. Negative values print as 00:00.
std::string formatDuration(Millis millis);

// "4 min", "38 s", "1 h 3 min" for short human summaries.
std::string formatSpan(Millis millis);

} // namespace ovc::timeutil
