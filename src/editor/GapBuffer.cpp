#include "editor/GapBuffer.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace ovc::editor {

GapBuffer::GapBuffer(std::size_t initialGap)
    : data_(initialGap)
    , gapStart_(0)
    , gapEnd_(initialGap)
{
}

char GapBuffer::at(std::size_t position) const
{
    if (position >= size())
        throw std::out_of_range("GapBuffer::at");
    return position < gapStart_ ? data_[position] : data_[position + gapSize()];
}

void GapBuffer::insert(std::size_t position, std::string_view text)
{
    if (position > size())
        throw std::out_of_range("GapBuffer::insert");
    if (text.empty())
        return;
    moveGapTo(position);
    ensureGap(text.size());
    std::memcpy(data_.data() + gapStart_, text.data(), text.size());
    gapStart_ += text.size();
}

void GapBuffer::erase(std::size_t position, std::size_t count)
{
    if (position > size() || count > size() - position)
        throw std::out_of_range("GapBuffer::erase");
    if (count == 0)
        return;
    moveGapTo(position);
    gapEnd_ += count; // the erased bytes simply become part of the gap
}

std::string GapBuffer::substr(std::size_t position, std::size_t count) const
{
    if (position > size())
        throw std::out_of_range("GapBuffer::substr");
    count = std::min(count, size() - position);
    std::string result;
    result.reserve(count);
    const std::size_t end = position + count;

    if (position < gapStart_) {
        const std::size_t beforeGapEnd = std::min(end, gapStart_);
        result.append(data_.data() + position, beforeGapEnd - position);
        position = beforeGapEnd;
    }
    if (position < end)
        result.append(data_.data() + position + gapSize(), end - position);
    return result;
}

std::string GapBuffer::toString() const
{
    return substr(0, size());
}

void GapBuffer::moveGapTo(std::size_t position)
{
    if (position < gapStart_) {
        // Shift the text between position and the gap to the gap's end.
        const std::size_t count = gapStart_ - position;
        std::memmove(data_.data() + gapEnd_ - count, data_.data() + position, count);
        gapStart_ -= count;
        gapEnd_ -= count;
    } else if (position > gapStart_) {
        const std::size_t count = position - gapStart_;
        std::memmove(data_.data() + gapStart_, data_.data() + gapEnd_, count);
        gapStart_ += count;
        gapEnd_ += count;
    }
}

void GapBuffer::ensureGap(std::size_t needed)
{
    if (gapSize() >= needed)
        return;
    const std::size_t afterGap = data_.size() - gapEnd_;
    const std::size_t newCapacity = std::max(data_.size() * 2, size() + needed + 64);
    std::vector<char> grown(newCapacity);
    std::memcpy(grown.data(), data_.data(), gapStart_);
    std::memcpy(grown.data() + newCapacity - afterGap, data_.data() + gapEnd_, afterGap);
    gapEnd_ = newCapacity - afterGap;
    data_.swap(grown);
}

} // namespace ovc::editor
