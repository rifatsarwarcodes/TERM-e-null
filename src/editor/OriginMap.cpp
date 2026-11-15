#include "editor/OriginMap.hpp"

#include <algorithm>
#include <stdexcept>

namespace ovc::editor {

std::size_t OriginMap::size() const
{
    std::size_t total = 0;
    for (const auto& span : spans_)
        total += span.length;
    return total;
}

std::size_t OriginMap::splitAt(std::size_t position)
{
    std::size_t start = 0;
    for (std::size_t i = 0; i < spans_.size(); ++i) {
        if (position == start)
            return i;
        const std::size_t end = start + spans_[i].length;
        if (position < end) {
            const std::size_t leftLength = position - start;
            Span right{spans_[i].length - leftLength, spans_[i].origin};
            spans_[i].length = leftLength;
            spans_.insert(spans_.begin() + static_cast<std::ptrdiff_t>(i) + 1, right);
            return i + 1;
        }
        start = end;
    }
    if (position == start)
        return spans_.size();
    throw std::out_of_range("OriginMap position past the end");
}

void OriginMap::insert(std::size_t position, std::size_t length, Origin origin)
{
    if (length == 0)
        return;
    const std::size_t index = splitAt(position);
    spans_.insert(spans_.begin() + static_cast<std::ptrdiff_t>(index), Span{length, origin});
    mergeNeighbours();
}

void OriginMap::erase(std::size_t position, std::size_t length)
{
    if (length == 0)
        return;
    const std::size_t first = splitAt(position);
    const std::size_t last = splitAt(position + length);
    spans_.erase(spans_.begin() + static_cast<std::ptrdiff_t>(first),
        spans_.begin() + static_cast<std::ptrdiff_t>(last));
    mergeNeighbours();
}

Origin OriginMap::at(std::size_t position) const
{
    std::size_t start = 0;
    for (const auto& span : spans_) {
        if (position < start + span.length)
            return span.origin;
        start += span.length;
    }
    throw std::out_of_range("OriginMap::at");
}

std::vector<OriginMap::Span> OriginMap::slice(std::size_t position, std::size_t length) const
{
    std::vector<Span> result;
    const std::size_t end = position + length;
    std::size_t start = 0;
    for (const auto& span : spans_) {
        const std::size_t spanEnd = start + span.length;
        const std::size_t from = std::max(start, position);
        const std::size_t to = std::min(spanEnd, end);
        if (from < to)
            result.push_back(Span{to - from, span.origin});
        if (spanEnd >= end)
            break;
        start = spanEnd;
    }
    return result;
}

std::size_t OriginMap::count(const Origin& origin) const
{
    std::size_t total = 0;
    for (const auto& span : spans_)
        if (span.origin == origin)
            total += span.length;
    return total;
}

void OriginMap::mergeNeighbours()
{
    std::vector<Span> merged;
    merged.reserve(spans_.size());
    for (const auto& span : spans_) {
        if (span.length == 0)
            continue;
        if (!merged.empty() && merged.back().origin == span.origin)
            merged.back().length += span.length;
        else
            merged.push_back(span);
    }
    spans_.swap(merged);
}

} // namespace ovc::editor
