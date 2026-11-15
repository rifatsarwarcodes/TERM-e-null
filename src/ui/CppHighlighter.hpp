#pragma once

#include "ui/Style.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace ovc::ui {

struct HighlightSpan {
    std::size_t start;  // byte offset in the line
    std::size_t length;
    Style style;
};

// A small line-based C++ highlighter: keywords, types, strings, character
// literals, numbers, comments and preprocessor lines. Block comments are
// tracked across lines through `inBlockComment`.
class CppHighlighter {
public:
    std::vector<HighlightSpan> highlight(const std::string& line, bool& inBlockComment) const;
};

} // namespace ovc::ui
