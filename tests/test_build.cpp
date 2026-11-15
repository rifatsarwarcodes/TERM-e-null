#include "TestFramework.hpp"

#include "build/BuildRunner.hpp"
#include "common/Files.hpp"

#include <filesystem>

using namespace ovc;
using namespace ovc::build;
namespace fs = std::filesystem;

TEST_CASE("compiler messages: unix and windows paths")
{
    const auto messages = CompilerMessageParser::parse(
        "main.cpp: In function 'int main()':\n"
        "main.cpp:12:5: error: expected ';' before 'return'\n"
        "C:\\lab\\main.cpp:3:10: warning: unused variable 'x' [-Wunused-variable]\n"
        "main.cpp:7: note: declared here\n"
        "collect2: error: ld returned 1 exit status\n");
    CHECK_EQ(messages.size(), std::size_t(3));
    CHECK_EQ(messages[0].file, std::string("main.cpp"));
    CHECK_EQ(messages[0].line, 12);
    CHECK_EQ(messages[0].column, 5);
    CHECK(messages[0].severity == Severity::Error);
    CHECK_EQ(messages[1].file, std::string("C:\\lab\\main.cpp"));
    CHECK(messages[1].severity == Severity::Warning);
    CHECK_EQ(messages[2].line, 7);
    CHECK(messages[2].severity == Severity::Note);
}

TEST_CASE("output comparison: judge-like rules")
{
    CHECK(OutputComparator::compare("YES\nNO\n", "YES  \r\nNO\n\n\n").matches);
    const Comparison wrong = OutputComparator::compare("YES\nNO\nYES", "YES\nYES\nYES");
    CHECK(!wrong.matches);
    CHECK(wrong.firstDifferentLine && *wrong.firstDifferentLine == 2);
    CHECK_EQ(wrong.expectedLine, std::string("NO"));
    const Comparison missing = OutputComparator::compare("1\n2", "1");
    CHECK(!missing.matches);
    CHECK_EQ(missing.actualLine, std::string("(no line)"));
}

namespace {

// Pretends to be the compiler and the program.
struct FakeRunner : platform::IProcessRunner {
    std::vector<platform::ProcessSpec> calls;
    platform::ProcessResult run(const platform::ProcessSpec& spec) override
    {
        calls.push_back(spec);
        platform::ProcessResult result;
        result.started = true;
        result.exitCode = 0;
        if (spec.program == "g++") {
            result.errors = "main.cpp:3:1: warning: something minor\n";
        } else if (spec.inputFile) {
            const std::string input = files::read(*spec.inputFile).value_or("");
            result.output = input == "1 2\n" ? "3\n" : "wrong\n";
        }
        return result;
    }
    platform::ProcessResult runInteractive(const platform::ProcessSpec& spec) override { return run(spec); }
};

} // namespace

TEST_CASE("build runner: compile passes flags and parses warnings")
{
    FakeRunner fake;
    BuildRunner runner(fake, BuildSettings{});
    const CompileResult result = runner.compile("main.cpp", "build/main");
    CHECK(result.ok);
    CHECK_EQ(result.warningCount(), 1);
    CHECK_EQ(fake.calls[0].arguments.back(), std::string("build/main"));
    CHECK_EQ(fake.calls[0].arguments.front(), std::string("-std=c++17"));
}

TEST_CASE("build runner: sample tests pass and fail")
{
    FakeRunner fake;
    BuildRunner runner(fake, BuildSettings{});
    std::vector<exam::TestCase> tests{{"test 1", "1 2", "3"}, {"test 2", "5 5", "10"}};
    const TestReport report = runner.runTests("main", tests, fs::temp_directory_path() / "ovc_test_build");
    CHECK_EQ(report.total(), 2);
    CHECK_EQ(report.passed(), 1);
    CHECK(report.cases[0].passed);
    CHECK(!report.cases[1].passed);
}

TEST_CASE("build settings come from the exam package")
{
    exam::ExamPackage package;
    package.compilerFlags = "-std=c++17  -O0";
    package.runTimeLimitSeconds = 3;
    const BuildSettings settings = BuildSettings::fromPackage(package);
    CHECK_EQ(settings.flags.size(), std::size_t(2));
    CHECK_EQ(settings.runTimeoutMs, 3000);
}

#ifndef _WIN32
TEST_CASE("process runner: captures output, input and exit code")
{
    auto runner = platform::createSystemProcessRunner();
    const fs::path input = fs::temp_directory_path() / "ovc_test_input.txt";
    files::writeAtomically(input, "hello\n");
    platform::ProcessSpec spec;
    spec.program = "/bin/sh";
    spec.arguments = {"-c", "read line; echo got $line; echo oops >&2; exit 3"};
    spec.inputFile = input;
    const auto result = runner->run(spec);
    CHECK(result.started);
    CHECK_EQ(result.output, std::string("got hello\n"));
    CHECK_EQ(result.errors, std::string("oops\n"));
    CHECK_EQ(result.exitCode, 3);
}

TEST_CASE("process runner: time limit stops a program that hangs")
{
    auto runner = platform::createSystemProcessRunner();
    platform::ProcessSpec spec;
    spec.program = "/bin/sh";
    spec.arguments = {"-c", "sleep 5"};
    spec.timeoutMs = 200;
    const auto result = runner->run(spec);
    CHECK(result.timedOut);
    CHECK(result.duration < 2000);
}

TEST_CASE("process runner: output limit stops endless printing")
{
    auto runner = platform::createSystemProcessRunner();
    platform::ProcessSpec spec;
    spec.program = "/bin/sh";
    spec.arguments = {"-c", "yes"};
    spec.timeoutMs = 5000;
    spec.outputLimit = 64 * 1024;
    const auto result = runner->run(spec);
    CHECK(result.outputLimited);
    CHECK(result.output.size() <= spec.outputLimit);
}

TEST_CASE("process runner: a missing program is reported")
{
    auto runner = platform::createSystemProcessRunner();
    platform::ProcessSpec spec;
    spec.program = "ovc-no-such-program";
    const auto result = runner->run(spec);
    CHECK(!result.started);
    CHECK(!result.failure.empty());
}
#endif
