#pragma once

#include "editor/Origin.hpp"

#include <cstddef>
#include <vector>

namespace ovc::editor {

// Remembers the origin of every character as runs ("spans") of equal origin.
// Typed text grows one span instead of creating one entry per character.
// Insertions and deletions keep the spans aligned with the text.
class OriginMap {
public:
    struct Span {
        std::size_t length;
        Origin origin;
    };

    void insert(std::size_t position, std::size_t length, Origin origin);
    void erase(std::size_t position, std::size_t length);
    void clear() { spans_.clear(); }

    Origin at(std::size_t position) const;
    std::vector<Span> slice(std::size_t position, std::size_t length) const;
    std::size_t size() const;
    const std::vector<Span>& spans() const { return spans_; }

    // Number of characters currently carrying exactly this origin.
    std::size_t count(const Origin& origin) const;

private:
    // Splits the span containing position so a span boundary lies there.
    // Returns the index of the span that starts at position.
    std::size_t splitAt(std::size_t position);
    void mergeNeighbours();

    std::vector<Span> spans_;
};

} // namespace ovc::editor
