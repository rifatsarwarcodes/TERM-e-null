#pragma once

#include "build/CompilerMessageParser.hpp"
#include "build/OutputComparator.hpp"
#include "exam/ExamPackage.hpp"
#include "platform/IProcessRunner.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace ovc::build {

namespace fs = std::filesystem;

struct BuildSettings {
    std::string compiler = "g++";
    std::vector<std::string> flags{"-std=c++17", "-O2", "-Wall"};
    int compileTimeoutMs = 60'000;
    int runTimeoutMs = 2'000;
    int interactiveTimeoutMs = 5 * 60'000;
    std::size_t outputLimit = 1 << 20;

    static BuildSettings fromPackage(const exam::ExamPackage& package);
};

struct CompileResult {
    bool ok = false;
    bool compilerMissing = false;
    std::vector<CompilerMessage> messages;
    std::string rawOutput;
    platform::ProcessResult process;
    int errorCount() const;
    int warningCount() const;
};

struct TestCaseResult {
    std::string name;
    bool passed = false;
    platform::ProcessResult process;
    Comparison comparison;
};

struct TestReport {
    std::vector<TestCaseResult> cases;
    int passed() const;
    int total() const { return static_cast<int>(cases.size()); }
    bool allPassed() const { return passed() == total(); }
};

// Compiles the student's file and runs the program or the sample tests.
class BuildRunner {
public:
    BuildRunner(platform::IProcessRunner& runner, BuildSettings settings);

    CompileResult compile(const fs::path& source, const fs::path& executable);
    platform::ProcessResult run(const fs::path& executable, const std::optional<fs::path>& inputFile);
    platform::ProcessResult runInteractive(const fs::path& executable);
    // Writes each test's input to `workDirectory` and compares the output.
    TestReport runTests(const fs::path& executable, const std::vector<exam::TestCase>& tests,
        const fs::path& workDirectory);

    const BuildSettings& settings() const { return settings_; }

private:
    platform::IProcessRunner& runner_;
    BuildSettings settings_;
};

// "main.exe" on Windows, "main" elsewhere.
fs::path executablePath(const fs::path& directory, const std::string& stem);

} // namespace ovc::build
