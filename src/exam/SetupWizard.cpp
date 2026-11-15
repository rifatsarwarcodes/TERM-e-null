#include "exam/SetupWizard.hpp"

#include "common/Files.hpp"
#include "common/Strings.hpp"
#include "exam/PackageParser.hpp"
#include "platform/Console.hpp"

#include <iostream>

namespace ovc::exam {

SetupWizard::SetupWizard(std::istream& in, std::ostream& out)
    : in_(in)
    , out_(out)
{
}

std::string SetupWizard::ask(const std::string& question, const std::string& fallback)
{
    out_ << question;
    if (!fallback.empty())
        out_ << " [" << fallback << "]";
    out_ << ": " << std::flush;
    std::string answer;
    if (!std::getline(in_, answer))
        return fallback;
    answer = strings::trim(answer);
    return answer.empty() ? fallback : answer;
}

int SetupWizard::askNumber(const std::string& question, int fallback)
{
    while (true) {
        const std::string answer = ask(question, std::to_string(fallback));
        long long value = 0;
        if (strings::parseInt(answer, value) && value >= 0)
            return static_cast<int>(value);
        if (!in_)
            return fallback;
        out_ << "  Please enter a whole number.\n";
    }
}

bool SetupWizard::askYesNo(const std::string& question, bool fallback)
{
    const std::string answer = strings::toLower(ask(question + " (y/n)", fallback ? "y" : "n"));
    return answer.empty() ? fallback : answer[0] == 'y';
}

std::string SetupWizard::askMultiline(const std::string& question)
{
    out_ << question << "\n  (finish with a line containing only a dot: .)\n";
    std::string text;
    std::string line;
    while (std::getline(in_, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line == ".")
            break;
        text += line + "\n";
    }
    if (!text.empty())
        text.pop_back();
    return text;
}

std::string SetupWizard::askPasscode()
{
    while (in_) {
        out_ << "Examiner passcode (not shown): " << std::flush;
        const std::string first = platform::readSecretLine(in_, out_);
        if (first.size() < 4) {
            out_ << "  Use at least 4 characters.\n";
            continue;
        }
        out_ << "Type it again: " << std::flush;
        const std::string second = platform::readSecretLine(in_, out_);
        if (first == second)
            return first;
        out_ << "  The two passcodes are different, try again.\n";
    }
    return {};
}

void SetupWizard::askTests(ExamPackage& package)
{
    while (askYesNo("Add a sample test?", package.tests.empty())) {
        TestCase test;
        test.name = "test " + std::to_string(package.tests.size() + 1);
        test.input = askMultiline("Input for " + test.name + ":");
        test.expectedOutput = askMultiline("Expected output for " + test.name + ":");
        package.tests.push_back(test);
    }
}

void SetupWizard::askAllowedFiles(ExamPackage& package, const std::filesystem::path& baseDirectory)
{
    while (askYesNo("Add an allowed file (template or starter code)?", false)) {
        const std::string name = ask("File path");
        const auto content = files::read(baseDirectory / name);
        if (!content) {
            out_ << "  Cannot read " << name << ".\n";
            continue;
        }
        package.allowed.push_back({name, *content});
    }
}

void SetupWizard::askPolicy(ExamPackage& package)
{
    penalty::PenaltyPolicy& p = package.policy;
    out_ << "\nPaste rules. Current values: large paste > " << p.largePasteChars << " chars or "
         << p.largePasteLines << " lines = " << p.largePastePoints << " points; warning at " << p.warningAt
         << ", flagged at " << p.flaggedAt << ".\n";
    if (!askYesNo("Change the rule values?", false))
        return;
    p.smallPasteChars = askNumber("Small paste limit (characters, recorded only)", p.smallPasteChars);
    p.largePasteChars = askNumber("Large paste (characters)", p.largePasteChars);
    p.largePasteLines = askNumber("Large paste (lines)", p.largePasteLines);
    p.largePastePoints = askNumber("Points for a large paste", p.largePastePoints);
    p.repeatCount = askNumber("Repeated pastes: more than how many", p.repeatCount);
    p.repeatWindowMinutes = askNumber("...within how many minutes", p.repeatWindowMinutes);
    p.repeatPoints = askNumber("Points for repeated pastes", p.repeatPoints);
    p.focusReturnSeconds = askNumber("Paste after coming back: within how many seconds", p.focusReturnSeconds);
    p.focusReturnPoints = askNumber("Points for that", p.focusReturnPoints);
    p.unchangedPoints = askNumber("Points for a paste left unchanged", p.unchangedPoints);
    p.warningAt = askNumber("Warning level at points", p.warningAt);
    p.flaggedAt = askNumber("Flagged level at points", p.flaggedAt);
}

bool SetupWizard::run(const std::filesystem::path& outputFile)
{
    out_ << "OV_C exam setup\n===============\nPress Enter to keep the value in [brackets].\n\n";
    ExamPackage package;
    package.code = ask("Exam code", "LAB-TEST");
    package.title = ask("Exam title", "Lab test");
    package.timeLimitMinutes = askNumber("Time limit in minutes (0 for none)", 90);
    package.sessionsRoot = ask("Folder for session data", "sessions/" + package.code);
    package.runTimeLimitSeconds = askNumber("Time limit for each program run (seconds)", 2);
    package.problem = askMultiline("\nProblem statement:");
    out_ << "\n";
    askTests(package);
    askAllowedFiles(package, outputFile.has_parent_path() ? outputFile.parent_path() : ".");
    askPolicy(package);
    out_ << "\n";

    const std::string passcode = askPasscode();
    if (passcode.empty()) {
        out_ << "No passcode given; nothing was written.\n";
        return false;
    }
    package.passcode = crypto::PasscodeHash::create(passcode);

    if (!files::writeAtomically(outputFile, PackageParser::serialize(package))) {
        out_ << "Could not write " << outputFile.string() << ".\n";
        return false;
    }
    out_ << "\nWrote " << outputFile.string() << ". Copy it next to the students' files.\n";
    return true;
}

} // namespace ovc::exam
