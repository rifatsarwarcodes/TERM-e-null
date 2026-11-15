#include "common/Time.hpp"

#include <chrono>
#include <cstdio>
#include <ctime>

namespace ovc::timeutil {

namespace {

std::tm localTime(Millis epochMillis)
{
    const std::time_t seconds = static_cast<std::time_t>(epochMillis / 1000);
    std::tm parts{};
#ifdef _WIN32
    localtime_s(&parts, &seconds);
#else
    localtime_r(&seconds, &parts);
#endif
    return parts;
}

} // namespace

Millis nowEpochMillis()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

Millis nowSteadyMillis()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

std::string formatClock(Millis epochMillis)
{
    const std::tm parts = localTime(epochMillis);
    char text[16];
    std::snprintf(text, sizeof text, "%02d:%02d:%02d", parts.tm_hour, parts.tm_min, parts.tm_sec);
    return text;
}

std::string formatDateTime(Millis epochMillis)
{
    const std::tm parts = localTime(epochMillis);
    char text[32];
    std::snprintf(text, sizeof text, "%04d-%02d-%02d %02d:%02d:%02d", parts.tm_year + 1900,
        parts.tm_mon + 1, parts.tm_mday, parts.tm_hour, parts.tm_min, parts.tm_sec);
    return text;
}

std::string formatDuration(Millis millis)
{
    if (millis < 0)
        millis = 0;
    const long long totalSeconds = millis / 1000;
    const long long hours = totalSeconds / 3600;
    const long long minutes = (totalSeconds / 60) % 60;
    const long long seconds = totalSeconds % 60;
    char text[32];
    if (hours > 0)
        std::snprintf(text, sizeof text, "%lld:%02lld:%02lld", hours, minutes, seconds);
    else
        std::snprintf(text, sizeof text, "%02lld:%02lld", minutes, seconds);
    return text;
}

std::string formatSpan(Millis millis)
{
    if (millis < 0)
        millis = 0;
    const long long totalSeconds = millis / 1000;
    if (totalSeconds < 60)
        return std::to_string(totalSeconds) + " s";
    const long long totalMinutes = totalSeconds / 60;
    if (totalMinutes < 60)
        return std::to_string(totalMinutes) + " min";
    return std::to_string(totalMinutes / 60) + " h " + std::to_string(totalMinutes % 60) + " min";
}

} // namespace ovc::timeutil
