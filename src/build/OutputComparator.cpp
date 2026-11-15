#include "build/OutputComparator.hpp"

#include "common/Strings.hpp"

#include <algorithm>

#include <vector>

namespace ovc::build {

namespace {

std::vector<std::string> normalisedLines(std::string_view text)
{
    std::vector<std::string> lines = strings::splitLines(text);
    for (auto& line : lines)
        line = strings::trimRight(line);
    while (!lines.empty() && lines.back().empty())
        lines.pop_back();
    return lines;
}

} // namespace

Comparison OutputComparator::compare(std::string_view expected, std::string_view actual)
{
    const auto want = normalisedLines(expected);
    const auto got = normalisedLines(actual);
    Comparison result;
    const std::size_t lines = std::max(want.size(), got.size());
    for (std::size_t i = 0; i < lines; ++i) {
        const std::string expectedLine = i < want.size() ? want[i] : "(no line)";
        const std::string actualLine = i < got.size() ? got[i] : "(no line)";
        if (i >= want.size() || i >= got.size() || want[i] != got[i]) {
            result.matches = false;
            result.firstDifferentLine = i + 1;
            result.expectedLine = expectedLine;
            result.actualLine = actualLine;
            break;
        }
    }
    return result;
}

} // namespace ovc::build
