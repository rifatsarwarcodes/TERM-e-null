#pragma once

#include <string>
#include <vector>

namespace ovc::ui {

// Breaks text into lines of at most `width` columns, at spaces where
// possible. Existing line breaks are kept.
std::vector<std::string> wrapText(const std::string& text, int width);

} // namespace ovc::ui
