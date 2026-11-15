#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace ovc::build {

struct Comparison {
    bool matches = true;
    std::optional<std::size_t> firstDifferentLine; // 1-based
    std::string expectedLine;
    std::string actualLine;
};

// Compares a program's output with the expected output line by line, like an
// online judge: trailing spaces on a line and trailing empty lines are ignored,
// and CRLF counts the same as LF.
class OutputComparator {
public:
    static Comparison compare(std::string_view expected, std::string_view actual);
};

} // namespace ovc::build
