#include "ui/TextWrap.hpp"

#include "common/Strings.hpp"

namespace ovc::ui {

namespace {

// Byte index after `columns` code points of `text` (or its end).
std::size_t byteIndexForColumns(const std::string& text, std::size_t columns)
{
    std::size_t i = 0;
    std::size_t seen = 0;
    while (i < text.size() && seen < columns) {
        ++i;
        while (i < text.size() && (static_cast<unsigned char>(text[i]) & 0xC0) == 0x80)
            ++i;
        ++seen;
    }
    return i;
}

} // namespace

std::vector<std::string> wrapText(const std::string& text, int width)
{
    std::vector<std::string> result;
    if (width <= 0)
        return result;
    const auto limit = static_cast<std::size_t>(width);

    for (std::string line : strings::splitLines(text)) {
        if (line.empty()) {
            result.emplace_back();
            continue;
        }
        while (strings::displayWidth(line) > limit) {
            const std::size_t cut = byteIndexForColumns(line, limit);
            std::size_t space = line.rfind(' ', cut);
            if (space == std::string::npos || space == 0)
                space = cut; // one long word: break it
            result.push_back(strings::trimRight(line.substr(0, space)));
            line = line.substr(space);
            if (!line.empty() && line[0] == ' ')
                line.erase(0, 1);
        }
        result.push_back(line);
    }
    return result;
}

} // namespace ovc::ui
