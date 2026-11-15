#include "editor/Motions.hpp"

#include <algorithm>
#include <cctype>

namespace ovc::editor::motions {

namespace {

enum class CharClass { Space, Word, Punctuation };

CharClass classify(char c)
{
    const auto byte = static_cast<unsigned char>(c);
    if (std::isspace(byte))
        return CharClass::Space;
    if (std::isalnum(byte) || c == '_' || byte >= 0x80)
        return CharClass::Word;
    return CharClass::Punctuation;
}

} // namespace

std::size_t left(const TextBuffer& buffer, std::size_t offset, std::size_t count)
{
    const std::size_t start = lineStart(buffer, offset);
    return offset - std::min(count, offset - start);
}

std::size_t right(const TextBuffer& buffer, std::size_t offset, std::size_t count)
{
    const std::size_t end = lineEnd(buffer, offset);
    return offset + std::min(count, end - offset);
}

std::size_t lineStart(const TextBuffer& buffer, std::size_t offset)
{
    return buffer.lineStart(buffer.positionOf(offset).line);
}

std::size_t lineEnd(const TextBuffer& buffer, std::size_t offset)
{
    const std::size_t line = buffer.positionOf(offset).line;
    return buffer.lineStart(line) + buffer.lineLength(line);
}

std::size_t firstNonBlank(const TextBuffer& buffer, std::size_t offset)
{
    std::size_t position = lineStart(buffer, offset);
    const std::size_t end = lineEnd(buffer, offset);
    while (position < end && (buffer.at(position) == ' ' || buffer.at(position) == '\t'))
        ++position;
    return position;
}

std::size_t vertical(const TextBuffer& buffer, std::size_t offset, long lines, std::size_t preferredColumn)
{
    const long current = static_cast<long>(buffer.positionOf(offset).line);
    const long last = static_cast<long>(buffer.lineCount()) - 1;
    const long target = std::clamp(current + lines, 0L, last);
    return buffer.offsetOf(static_cast<std::size_t>(target), preferredColumn);
}

std::size_t wordForward(const TextBuffer& buffer, std::size_t offset, std::size_t count)
{
    const std::size_t size = buffer.size();
    for (std::size_t n = 0; n < count && offset < size; ++n) {
        const CharClass start = classify(buffer.at(offset));
        if (start != CharClass::Space)
            while (offset < size && classify(buffer.at(offset)) == start)
                ++offset;
        while (offset < size && classify(buffer.at(offset)) == CharClass::Space)
            ++offset;
    }
    return offset;
}

std::size_t wordBackward(const TextBuffer& buffer, std::size_t offset, std::size_t count)
{
    for (std::size_t n = 0; n < count && offset > 0; ++n) {
        --offset;
        while (offset > 0 && classify(buffer.at(offset)) == CharClass::Space)
            --offset;
        const CharClass cls = classify(buffer.at(offset));
        while (offset > 0 && classify(buffer.at(offset - 1)) == cls)
            --offset;
    }
    return offset;
}

std::size_t wordEnd(const TextBuffer& buffer, std::size_t offset, std::size_t count)
{
    const std::size_t size = buffer.size();
    for (std::size_t n = 0; n < count && offset + 1 < size; ++n) {
        ++offset;
        while (offset + 1 < size && classify(buffer.at(offset)) == CharClass::Space)
            ++offset;
        const CharClass cls = classify(buffer.at(offset));
        while (offset + 1 < size && classify(buffer.at(offset + 1)) == cls)
            ++offset;
    }
    return offset;
}

std::string indentationOf(const TextBuffer& buffer, std::size_t offset)
{
    const std::size_t start = lineStart(buffer, offset);
    return buffer.substr(start, firstNonBlank(buffer, offset) - start);
}

} // namespace ovc::editor::motions
