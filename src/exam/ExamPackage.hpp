#pragma once

#include "crypto/PasscodeHash.hpp"
#include "penalty/PenaltyPolicy.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace ovc::exam {

struct TestCase {
    std::string name;
    std::string input;
    std::string expectedOutput;
};

struct AllowedSnippet {
    std::string name; // file name or "snippet N"
    std::string text;
};

// Everything the teacher prepares for one exam, read from a plain text file
// (see docs/exam-package.md). Without a package OV_C runs as a practice
// session with the defaults below.
struct ExamPackage {
    std::string code = "PRACTICE";
    std::string title = "Practice session";
    std::string problem;
    int timeLimitMinutes = 0; // 0: no limit
    std::string sessionsRoot = "sessions";
    std::string compiler = "g++";
    std::string compilerFlags = "-std=c++17 -O2 -Wall";
    int runTimeLimitSeconds = 2;
    std::optional<crypto::PasscodeHash> passcode;
    std::vector<TestCase> tests;
    std::vector<AllowedSnippet> allowed;
    penalty::PenaltyPolicy policy;

    std::filesystem::path sourceFile; // empty for the practice defaults
    std::string sourceHash;           // SHA-256 of the package file

    bool isPractice() const { return sourceFile.empty(); }
};

} // namespace ovc::exam
