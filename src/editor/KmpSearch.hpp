#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ovc::editor {

// Knuth-Morris-Pratt string search. The failure table is built once per
// pattern, then a text is scanned in O(n + m) without stepping back.
class KmpSearch {
public:
    explicit KmpSearch(std::string pattern);

    const std::string& pattern() const { return pattern_; }

    std::vector<std::size_t> findAll(std::string_view text) const;

    // First match starting at or after `from`; wraps around to the start.
    std::optional<std::size_t> findNext(std::string_view text, std::size_t from) const;

    // Last match starting before `from`; wraps around to the end.
    std::optional<std::size_t> findPrevious(std::string_view text, std::size_t from) const;

private:
    std::string pattern_;
    std::vector<std::size_t> failure_; // longest proper prefix that is also a suffix
};

} // namespace ovc::editor
