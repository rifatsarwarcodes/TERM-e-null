#pragma once

#include "dashboard/SessionRecord.hpp"
#include "exam/ExamPackage.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace ovc::dashboard {

// Reads a session folder and works out everything the dashboard shows:
//
//   1. load the events and the journal
//   2. verify both hash chains, the seal, the receipt and the submitted file
//   3. replay the journal to rebuild the final code with its origins
//   4. recalculate the penalties with the examiner's policy
//   5. build the viva focus list
//
// The dashboard never writes into the session folder (reports go next to it
// only when the examiner exports one).
class SessionLoader {
public:
    // `examinerPackage` is the examiner's own copy; its policy is used and its
    // hash is compared with the one the student's session recorded.
    static SessionRecord load(const std::filesystem::path& folder, const exam::ExamPackage* examinerPackage);
};

} // namespace ovc::dashboard
