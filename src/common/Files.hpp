#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace ovc::files {

namespace fs = std::filesystem;

std::optional<std::string> read(const fs::path& path);

// Writes to a temporary file next to the target and renames it over the
// target, so a crash never leaves a half-written file behind.
bool writeAtomically(const fs::path& path, std::string_view content);

bool ensureDirectory(const fs::path& path);

} // namespace ovc::files
