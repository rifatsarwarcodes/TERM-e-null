#include "build/BuildRunner.hpp"

#include "common/Files.hpp"
#include "common/Strings.hpp"

#include <algorithm>

namespace ovc::build {

BuildSettings BuildSettings::fromPackage(const exam::ExamPackage& package)
{
    BuildSettings settings;
    settings.compiler = package.compiler;
    settings.flags.clear();
    for (const auto& flag : strings::split(package.compilerFlags, ' '))
        if (!flag.empty())
            settings.flags.push_back(flag);
    settings.runTimeoutMs = std::max(1, package.runTimeLimitSeconds) * 1000;
    return settings;
}

int CompileResult::errorCount() const
{
    int count = 0;
    for (const auto& message : messages)
        if (message.severity == Severity::Error)
            ++count;
    return count;
}

int CompileResult::warningCount() const
{
    int count = 0;
    for (const auto& message : messages)
        if (message.severity == Severity::Warning)
            ++count;
    return count;
}

int TestReport::passed() const
{
    int count = 0;
    for (const auto& result : cases)
        if (result.passed)
            ++count;
    return count;
}

BuildRunner::BuildRunner(platform::IProcessRunner& runner, BuildSettings settings)
    : runner_(runner)
    , settings_(std::move(settings))
{
}

CompileResult BuildRunner::compile(const fs::path& source, const fs::path& executable)
{
    platform::ProcessSpec spec;
    spec.program = settings_.compiler;
    spec.arguments = settings_.flags;
    spec.arguments.push_back(source.string());
    spec.arguments.push_back("-o");
    spec.arguments.push_back(executable.string());
    spec.timeoutMs = settings_.compileTimeoutMs;
    spec.outputLimit = settings_.outputLimit;

    CompileResult result;
    result.process = runner_.run(spec);
    if (!result.process.started) {
        result.compilerMissing = true;
        result.rawOutput = result.process.failure;
        return result;
    }
    result.rawOutput = result.process.errors + result.process.output;
    result.messages = CompilerMessageParser::parse(result.rawOutput);
    result.ok = result.process.succeeded();
    return result;
}

platform::ProcessResult BuildRunner::run(const fs::path& executable, const std::optional<fs::path>& inputFile)
{
    platform::ProcessSpec spec;
    spec.program = executable.string();
    spec.inputFile = inputFile;
    spec.timeoutMs = settings_.runTimeoutMs;
    spec.outputLimit = settings_.outputLimit;
    return runner_.run(spec);
}

platform::ProcessResult BuildRunner::runInteractive(const fs::path& executable)
{
    platform::ProcessSpec spec;
    spec.program = executable.string();
    spec.timeoutMs = settings_.interactiveTimeoutMs;
    return runner_.runInteractive(spec);
}

TestReport BuildRunner::runTests(const fs::path& executable, const std::vector<exam::TestCase>& tests,
    const fs::path& workDirectory)
{
    TestReport report;
    files::ensureDirectory(workDirectory);
    for (std::size_t i = 0; i < tests.size(); ++i) {
        const fs::path input = workDirectory / ("test" + std::to_string(i + 1) + ".in");
        std::string text = tests[i].input;
        if (!text.empty() && text.back() != '\n')
            text += '\n';
        files::writeAtomically(input, text);

        TestCaseResult result;
        result.name = tests[i].name.empty() ? "test " + std::to_string(i + 1) : tests[i].name;
        result.process = run(executable, input);
        result.comparison = OutputComparator::compare(tests[i].expectedOutput, result.process.output);
        result.passed = result.process.succeeded() && result.comparison.matches;
        report.cases.push_back(std::move(result));
    }
    return report;
}

fs::path executablePath(const fs::path& directory, const std::string& stem)
{
#ifdef _WIN32
    return directory / (stem + ".exe");
#else
    return directory / stem;
#endif
}

} // namespace ovc::build
