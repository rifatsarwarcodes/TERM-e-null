#pragma once

#include "common/Time.hpp"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace ovc::platform {

struct ProcessSpec {
    std::string program;                 // searched on PATH if it has no directory
    std::vector<std::string> arguments;
    std::optional<std::filesystem::path> inputFile; // standard input for captured runs
    int timeoutMs = 2000;
    std::size_t outputLimit = 1 << 20;   // bytes kept from stdout and stderr each
};

struct ProcessResult {
    bool started = false;
    std::string failure;     // why it could not be started
    int exitCode = -1;
    bool timedOut = false;   // stopped because it ran too long
    bool outputLimited = false; // stopped because it printed too much
    std::string output;      // standard output
    std::string errors;      // standard error
    timeutil::Millis duration = 0;

    bool succeeded() const { return started && !timedOut && !outputLimited && exitCode == 0; }
};

// Starts other programs: the compiler and the student's program. Output goes
// to temporary files rather than pipes, so a program that prints a lot can
// never block, and a time limit stops programs that hang, together with any
// processes they started.
class IProcessRunner {
public:
    virtual ~IProcessRunner() = default;

    // Runs with captured output.
    virtual ProcessResult run(const ProcessSpec& spec) = 0;

    // Runs attached to the console so the user can type input. Output is not
    // captured; exit code, duration and time limit still apply.
    virtual ProcessResult runInteractive(const ProcessSpec& spec) = 0;
};

std::unique_ptr<IProcessRunner> createSystemProcessRunner();

} // namespace ovc::platform
