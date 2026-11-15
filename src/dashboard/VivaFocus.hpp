#pragma once

#include "dashboard/SessionRecord.hpp"

#include <vector>

namespace ovc::dashboard {

// Builds the list of code sections worth asking about at the viva:
//   - outside pastes that are still in the final code (highest priority when
//     mostly unchanged)
//   - code written shortly after the student came back to the window
//   - code that changed between a failing test and the next passing test
// Character ranges are turned into line ranges and overlapping ranges with
// the same kind of reason are merged.
class VivaFocusBuilder {
public:
    static std::vector<FocusItem> build(const SessionRecord& record);
};

// Merges overlapping or touching line ranges that share a reason prefix.
std::vector<FocusItem> mergeRanges(std::vector<FocusItem> items);

} // namespace ovc::dashboard
