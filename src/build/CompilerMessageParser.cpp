#include "build/CompilerMessageParser.hpp"

#include "common/Strings.hpp"

namespace ovc::build {

namespace {

struct Marker {
    const char* text;
    Severity severity;
};

constexpr Marker Markers[] = {
    {": fatal error: ", Severity::Error},
    {": error: ", Severity::Error},
    {": warning: ", Severity::Warning},
    {": note: ", Severity::Note},
};

// Reads the number that ends just before `end` (exclusive), moving `end` to
// its first digit. Returns -1 if there is no number.
int numberBefore(const std::string& line, std::size_t& end)
{
    std::size_t start = end;
    while (start > 0 && line[start - 1] >= '0' && line[start - 1] <= '9')
        --start;
    if (start == end)
        return -1;
    long long value = 0;
    strings::parseInt(line.substr(start, end - start), value);
    end = start;
    return static_cast<int>(value);
}

} // namespace

std::vector<CompilerMessage> CompilerMessageParser::parse(std::string_view output)
{
    std::vector<CompilerMessage> messages;
    for (const auto& line : strings::splitLines(output)) {
        for (const auto& marker : Markers) {
            const std::size_t at = line.find(marker.text);
            if (at == std::string::npos)
                continue;
            // Expect "<file>:<line>:<column>" (or "<file>:<line>") before the marker.
            std::size_t cursor = at;
            int column = numberBefore(line, cursor);
            if (column < 0 || cursor == 0 || line[cursor - 1] != ':')
                break;
            --cursor;
            int lineNumber = numberBefore(line, cursor);
            if (lineNumber < 0) {
                lineNumber = column; // only "<file>:<line>"
                column = 0;
                ++cursor;
            } else {
                if (cursor == 0 || line[cursor - 1] != ':')
                    break;
                --cursor;
            }
            CompilerMessage message;
            message.file = line.substr(0, cursor);
            message.line = lineNumber;
            message.column = column;
            message.severity = marker.severity;
            message.text = line.substr(at + std::char_traits<char>::length(marker.text));
            messages.push_back(message);
            break;
        }
    }
    return messages;
}

const char* severityName(Severity severity)
{
    switch (severity) {
    case Severity::Error: return "error";
    case Severity::Warning: return "warning";
    case Severity::Note: return "note";
    }
    return "error";
}

} // namespace ovc::build
