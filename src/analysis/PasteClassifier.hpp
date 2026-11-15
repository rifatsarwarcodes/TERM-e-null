#pragma once

#include "analysis/Fingerprint.hpp"
#include "editor/OriginMap.hpp"
#include "editor/TextBuffer.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace ovc::analysis {

enum class PasteSource { Self, Allowed, Outside };

const char* pasteSourceName(PasteSource source);
editor::OriginKind originKindFor(PasteSource source);

// Remembers text that was deleted during the session. A student who deletes
// a function and later pastes it back is pasting their own work.
class RemovedTextHistory final : public editor::IEditObserver {
public:
    void onInsert(std::size_t, const std::string&, const editor::Origin&) override {}
    void onErase(std::size_t, const std::string& removed) override { removed_ += removed + "\n"; }
    const std::string& text() const { return removed_; }

private:
    std::string removed_;
};

// Decides where a paste that came through the terminal is from:
//   self    - the text already existed in this session (buffer or deleted text)
//   allowed - it matches a snippet the teacher allowed
//   outside - anything else
class PasteClassifier {
public:
    explicit PasteClassifier(const std::vector<std::string>& allowedSnippets, double threshold = 0.8);

    PasteSource classify(std::string_view pasted, std::string_view sessionText) const;

private:
    FingerprintIndex allowed_;
    double threshold_;
};

// Share of a paste that is still in the document unchanged (0.0 to 1.0).
double retention(const editor::OriginMap& origins, const editor::Origin& pasteOrigin, std::size_t pastedLength);

} // namespace ovc::analysis
