#include "editor/TextBuffer.hpp"

#include <algorithm>
#include <stdexcept>

namespace ovc::editor {

void TextBuffer::insert(std::size_t position, std::string_view text, const Origin& origin)
{
    if (text.empty())
        return;
    chars_.insert(position, text);
    origins_.insert(position, text.size(), origin);
    lineIndexDirty_ = true;

    const std::string inserted(text);
    for (auto* observer : observers_)
        observer->onInsert(position, inserted, origin);
}

std::string TextBuffer::erase(std::size_t position, std::size_t count)
{
    if (count == 0)
        return {};
    std::string removed = chars_.substr(position, count);
    chars_.erase(position, count);
    origins_.erase(position, count);
    lineIndexDirty_ = true;

    for (auto* observer : observers_)
        observer->onErase(position, removed);
    return removed;
}

std::string TextBuffer::substr(std::size_t position, std::size_t count) const
{
    return chars_.substr(position, count);
}

void TextBuffer::ensureLineIndex() const
{
    if (!lineIndexDirty_)
        return;
    lineStarts_.assign(1, 0);
    const std::string all = chars_.toString();
    for (std::size_t i = 0; i < all.size(); ++i)
        if (all[i] == '\n')
            lineStarts_.push_back(i + 1);
    lineIndexDirty_ = false;
}

std::size_t TextBuffer::lineCount() const
{
    ensureLineIndex();
    return lineStarts_.size();
}

std::size_t TextBuffer::lineStart(std::size_t line) const
{
    ensureLineIndex();
    if (line >= lineStarts_.size())
        throw std::out_of_range("TextBuffer::lineStart");
    return lineStarts_[line];
}

std::size_t TextBuffer::lineLength(std::size_t line) const
{
    ensureLineIndex();
    const std::size_t start = lineStart(line);
    const std::size_t end = line + 1 < lineStarts_.size() ? lineStarts_[line + 1] - 1 : size();
    return end - start;
}

std::string TextBuffer::line(std::size_t line) const
{
    return substr(lineStart(line), lineLength(line));
}

TextPosition TextBuffer::positionOf(std::size_t offset) const
{
    ensureLineIndex();
    offset = std::min(offset, size());
    // Binary search for the last line start that is <= offset.
    const auto next = std::upper_bound(lineStarts_.begin(), lineStarts_.end(), offset);
    const std::size_t line = static_cast<std::size_t>(next - lineStarts_.begin()) - 1;
    return {line, offset - lineStarts_[line]};
}

std::size_t TextBuffer::offsetOf(std::size_t line, std::size_t column) const
{
    const std::size_t lines = lineCount();
    if (line >= lines)
        line = lines - 1;
    return lineStart(line) + std::min(column, lineLength(line));
}

std::vector<OriginMap::Span> TextBuffer::originsIn(std::size_t position, std::size_t count) const
{
    return origins_.slice(position, count);
}

void TextBuffer::addObserver(IEditObserver* observer)
{
    if (std::find(observers_.begin(), observers_.end(), observer) == observers_.end())
        observers_.push_back(observer);
}

void TextBuffer::removeObserver(IEditObserver* observer)
{
    observers_.erase(std::remove(observers_.begin(), observers_.end(), observer), observers_.end());
}

} // namespace ovc::editor
