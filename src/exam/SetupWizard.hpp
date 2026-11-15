#pragma once

#include "exam/ExamPackage.hpp"

#include <filesystem>
#include <iosfwd>

namespace ovc::exam {

// The teacher's terminal questionnaire behind "ovc --setup". It asks for the
// exam details, problem statement, sample tests, allowed files, rule values
// and the examiner passcode, then writes the exam package.
class SetupWizard {
public:
    SetupWizard(std::istream& in, std::ostream& out);

    // Returns false if the input ended early or the file could not be written.
    bool run(const std::filesystem::path& outputFile);

private:
    std::string ask(const std::string& question, const std::string& fallback = "");
    int askNumber(const std::string& question, int fallback);
    bool askYesNo(const std::string& question, bool fallback);
    std::string askMultiline(const std::string& question);
    std::string askPasscode();
    void askTests(ExamPackage& package);
    void askAllowedFiles(ExamPackage& package, const std::filesystem::path& baseDirectory);
    void askPolicy(ExamPackage& package);

    std::istream& in_;
    std::ostream& out_;
};

} // namespace ovc::exam
