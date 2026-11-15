#include "editor/KmpSearch.hpp"

namespace ovc::editor {

KmpSearch::KmpSearch(std::string pattern)
    : pattern_(std::move(pattern))
    , failure_(pattern_.size(), 0)
{
    std::size_t matched = 0;
    for (std::size_t i = 1; i < pattern_.size(); ++i) {
        while (matched > 0 && pattern_[i] != pattern_[matched])
            matched = failure_[matched - 1];
        if (pattern_[i] == pattern_[matched])
            ++matched;
        failure_[i] = matched;
    }
}

std::vector<std::size_t> KmpSearch::findAll(std::string_view text) const
{
    std::vector<std::size_t> matches;
    if (pattern_.empty())
        return matches;
    std::size_t matched = 0;
    for (std::size_t i = 0; i < text.size(); ++i) {
        while (matched > 0 && text[i] != pattern_[matched])
            matched = failure_[matched - 1];
        if (text[i] == pattern_[matched])
            ++matched;
        if (matched == pattern_.size()) {
            matches.push_back(i + 1 - matched);
            matched = failure_[matched - 1];
        }
    }
    return matches;
}

std::optional<std::size_t> KmpSearch::findNext(std::string_view text, std::size_t from) const
{
    const auto matches = findAll(text);
    if (matches.empty())
        return std::nullopt;
    for (std::size_t match : matches)
        if (match >= from)
            return match;
    return matches.front();
}

std::optional<std::size_t> KmpSearch::findPrevious(std::string_view text, std::size_t from) const
{
    const auto matches = findAll(text);
    if (matches.empty())
        return std::nullopt;
    for (auto it = matches.rbegin(); it != matches.rend(); ++it)
        if (*it < from)
            return *it;
    return matches.back();
}

} // namespace ovc::editor
