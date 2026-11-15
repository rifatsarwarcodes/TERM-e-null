#pragma once

#include "exam/ExamPackage.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace ovc::exam {

struct ParseResult {
    ExamPackage package;
    std::vector<std::string> errors;
    bool ok() const { return errors.empty(); }
};

// Reads and writes the exam package format:
//
//   [exam]                 key = value lines
//   [problem]              free text until the next [section]
//   [test 1]               input <<< ... >>>   and   output <<< ... >>>
//   [allowed]              file = template.cpp   or   snippet <<< ... >>>
//   [policy]               key = value lines (see PenaltyPolicy)
//
// Lines starting with '#' outside text blocks are comments.
class PackageParser {
public:
    // `baseDirectory` is used to read allowed files given by relative path.
    static ParseResult parse(const std::string& text, const std::filesystem::path& baseDirectory);
    static std::string serialize(const ExamPackage& package);
};

// Loads a package file and records its path and hash.
ParseResult loadPackage(const std::filesystem::path& path);

} // namespace ovc::exam
