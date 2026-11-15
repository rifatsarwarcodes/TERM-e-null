#pragma once

#include "platform/InputEvent.hpp"

#include <cstddef>
#include <vector>

namespace ovc::analysis {

// Fallback paste detection for consoles without bracketed paste.
//
// A person typing rarely has more than two or three keys waiting in the
// input queue; a paste puts the whole text there at once. The terminal hands
// over everything that was waiting as one batch, so a long run of text keys
// inside a single batch is turned into one (probable) PasteEvent. Runs of the
// same repeated character (a held-down key) are left alone.
class BurstDetector {
public:
    explicit BurstDetector(std::size_t minimumKeys = 8);

    std::vector<platform::InputEvent> process(std::vector<platform::InputEvent> batch) const;

private:
    std::size_t minimumKeys_;
};

} // namespace ovc::analysis
