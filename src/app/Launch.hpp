#pragma once

#include "app/CommandLine.hpp"
#include "exam/ExamPackage.hpp"

#include <iosfwd>
#include <optional>

namespace ovc::app {

// Loads the exam package from --exam, or exam.ovx in the current folder, or
// falls back to a practice session. Prints problems to `err`.
std::optional<exam::ExamPackage> loadExamPackage(const CommandLine& commandLine, std::ostream& err);

int runStudent(const CommandLine& commandLine);
int runExaminer(const CommandLine& commandLine);
int runSetup(const CommandLine& commandLine);

} // namespace ovc::app
