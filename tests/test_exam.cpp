#include "TestFramework.hpp"

#include "common/Files.hpp"
#include "exam/PackageParser.hpp"
#include "exam/SetupWizard.hpp"

#include <filesystem>
#include <sstream>

using namespace ovc;
using namespace ovc::exam;
namespace fs = std::filesystem;

namespace {

const char* SamplePackage = R"(# Lab test 2
[exam]
code = SWE4304-LT2
title = Lab Test 2: Balanced Brackets
time_limit = 90
sessions = sessions/LT2

[problem]
Read q strings made only of ()[]{}.
Print YES if balanced.

[test 1]
input <<<
2
()
(]
>>>
output <<<
YES
NO
>>>

[test 2]
input = 1\n{}
output = YES

[allowed]
snippet <<<
int main() {
}
>>>

[policy]
large_paste_chars = 250
unchanged_ratio = 0.75
flagged_at = 10
)";

} // namespace

TEST_CASE("package: parses every section")
{
    const ParseResult result = PackageParser::parse(SamplePackage, ".");
    CHECK(result.ok());
    const ExamPackage& p = result.package;
    CHECK_EQ(p.code, std::string("SWE4304-LT2"));
    CHECK_EQ(p.timeLimitMinutes, 90);
    CHECK_EQ(p.problem, std::string("Read q strings made only of ()[]{}.\nPrint YES if balanced."));
    CHECK_EQ(p.tests.size(), std::size_t(2));
    CHECK_EQ(p.tests[0].input, std::string("2\n()\n(]"));
    CHECK_EQ(p.tests[1].input, std::string("1\n{}"));
    CHECK_EQ(p.allowed.size(), std::size_t(1));
    CHECK_EQ(p.policy.largePasteChars, 250);
    CHECK_EQ(p.policy.flaggedAt, 10);
    CHECK(p.policy.unchangedRatio > 0.74 && p.policy.unchangedRatio < 0.76);
}

TEST_CASE("package: reports mistakes with line numbers")
{
    const ParseResult result = PackageParser::parse("[exam]\ntime_limit = soon\n[policy]\nmystery = 3\n", ".");
    CHECK_EQ(result.errors.size(), std::size_t(2));
    CHECK(result.errors[0].find("line 2") != std::string::npos);
}

TEST_CASE("package: unclosed block is an error")
{
    const ParseResult result = PackageParser::parse("[test 1]\ninput <<<\n1\n", ".");
    CHECK(!result.ok());
}

TEST_CASE("package: serialize and parse again gives the same package")
{
    ExamPackage original = PackageParser::parse(SamplePackage, ".").package;
    original.passcode = crypto::PasscodeHash::create("teacher", 10);
    const ParseResult again = PackageParser::parse(PackageParser::serialize(original), ".");
    CHECK(again.ok());
    CHECK_EQ(again.package.tests.size(), original.tests.size());
    CHECK_EQ(again.package.tests[0].expectedOutput, original.tests[0].expectedOutput);
    CHECK_EQ(again.package.problem, original.problem);
    CHECK(again.package.passcode && again.package.passcode->matches("teacher"));
    CHECK_EQ(again.package.policy.largePasteChars, 250);
}

TEST_CASE("package: loading records the file hash")
{
    const fs::path path = fs::temp_directory_path() / "ovc_test_exam.ovx";
    files::writeAtomically(path, SamplePackage);
    const ParseResult result = loadPackage(path);
    CHECK(result.ok());
    CHECK_EQ(result.package.sourceHash.size(), std::size_t(64));
    CHECK(!result.package.isPractice());
}

TEST_CASE("setup wizard: answers become a package file")
{
    std::istringstream answers(
        "LT2\n"            // code
        "Lab Test 2\n"     // title
        "60\n"             // minutes
        "\n"               // sessions folder: default
        "\n"               // run limit: default
        "Sum two numbers.\n.\n"
        "y\n1 2\n.\n3\n.\n" // one test
        "n\n"              // no more tests
        "n\n"              // no allowed files
        "n\n"              // keep rule values
        "pass1\npass1\n");
    std::ostringstream screen;
    const fs::path path = fs::temp_directory_path() / "ovc_test_wizard.ovx";
    fs::remove(path);
    SetupWizard wizard(answers, screen);
    CHECK(wizard.run(path));

    const ParseResult result = loadPackage(path);
    CHECK(result.ok());
    CHECK_EQ(result.package.code, std::string("LT2"));
    CHECK_EQ(result.package.timeLimitMinutes, 60);
    CHECK_EQ(result.package.sessionsRoot, std::string("sessions/LT2"));
    CHECK_EQ(result.package.tests.size(), std::size_t(1));
    CHECK_EQ(result.package.tests[0].expectedOutput, std::string("3"));
    CHECK(result.package.passcode && result.package.passcode->matches("pass1"));
}
