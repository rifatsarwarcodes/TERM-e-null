#include "analysis/PasteClassifier.hpp"

#include <algorithm>

namespace ovc::analysis {

const char* pasteSourceName(PasteSource source)
{
    switch (source) {
    case PasteSource::Self: return "self";
    case PasteSource::Allowed: return "allowed";
    case PasteSource::Outside: return "outside";
    }
    return "outside";
}

editor::OriginKind originKindFor(PasteSource source)
{
    switch (source) {
    case PasteSource::Self: return editor::OriginKind::Self;
    case PasteSource::Allowed: return editor::OriginKind::Allowed;
    case PasteSource::Outside: return editor::OriginKind::Outside;
    }
    return editor::OriginKind::Outside;
}

PasteClassifier::PasteClassifier(const std::vector<std::string>& allowedSnippets, double threshold)
    : threshold_(threshold)
{
    for (const auto& snippet : allowedSnippets)
        allowed_.add(snippet);
}

PasteSource PasteClassifier::classify(std::string_view pasted, std::string_view sessionText) const
{
    FingerprintIndex session;
    session.add(sessionText);
    if (session.coverage(pasted) >= threshold_)
        return PasteSource::Self;
    if (allowed_.coverage(pasted) >= threshold_)
        return PasteSource::Allowed;
    return PasteSource::Outside;
}

double retention(const editor::OriginMap& origins, const editor::Origin& pasteOrigin, std::size_t pastedLength)
{
    if (pastedLength == 0)
        return 0.0;
    const double kept = static_cast<double>(origins.count(pasteOrigin));
    return std::min(1.0, kept / static_cast<double>(pastedLength));
}

} // namespace ovc::analysis
