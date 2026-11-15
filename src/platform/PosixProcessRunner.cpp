// Process runner for Linux and macOS: fork/exec with output in temporary
// files and a process group so the whole tree can be stopped.
#ifndef _WIN32

#include "common/Files.hpp"
#include "platform/IProcessRunner.hpp"

#include <cerrno>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

namespace ovc::platform {

namespace {

namespace fs = std::filesystem;

std::vector<char*> argumentVector(const ProcessSpec& spec, std::vector<std::string>& storage)
{
    storage.clear();
    storage.push_back(spec.program);
    storage.insert(storage.end(), spec.arguments.begin(), spec.arguments.end());
    std::vector<char*> argv;
    for (auto& text : storage)
        argv.push_back(text.data());
    argv.push_back(nullptr);
    return argv;
}

fs::path temporaryFile(const char* tag)
{
    static int counter = 0;
    return fs::temp_directory_path()
        / ("ovc_" + std::to_string(getpid()) + "_" + std::to_string(++counter) + "_" + tag + ".txt");
}

std::uintmax_t sizeOf(const fs::path& path)
{
    std::error_code error;
    const auto size = fs::file_size(path, error);
    return error ? 0 : size;
}

std::string readLimited(const fs::path& path, std::size_t limit, bool& limited)
{
    std::string text = files::read(path).value_or("");
    if (text.size() > limit) {
        text.resize(limit);
        limited = true;
    }
    return text;
}

// Waits for the child; stops its process group on timeout or too much output.
void waitFor(pid_t child, const ProcessSpec& spec, ProcessResult& result, const fs::path* outFile,
    const fs::path* errFile)
{
    const timeutil::Millis start = timeutil::nowSteadyMillis();
    int status = 0;
    while (true) {
        const pid_t done = waitpid(child, &status, WNOHANG);
        if (done == child)
            break;
        if (done < 0 && errno != EINTR)
            break;
        const timeutil::Millis elapsed = timeutil::nowSteadyMillis() - start;
        bool stop = false;
        if (spec.timeoutMs > 0 && elapsed > spec.timeoutMs) {
            result.timedOut = true;
            stop = true;
        }
        if (outFile != nullptr && (sizeOf(*outFile) > spec.outputLimit || sizeOf(*errFile) > spec.outputLimit)) {
            result.outputLimited = true;
            stop = true;
        }
        if (stop) {
            kill(-child, SIGKILL);
            kill(child, SIGKILL);
            waitpid(child, &status, 0);
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    result.duration = timeutil::nowSteadyMillis() - start;
    if (WIFEXITED(status))
        result.exitCode = WEXITSTATUS(status);
    else if (WIFSIGNALED(status))
        result.exitCode = 128 + WTERMSIG(status);
}

class PosixProcessRunner final : public IProcessRunner {
public:
    ProcessResult run(const ProcessSpec& spec) override
    {
        ProcessResult result;
        const fs::path outFile = temporaryFile("out");
        const fs::path errFile = temporaryFile("err");
        std::vector<std::string> storage;
        auto argv = argumentVector(spec, storage);

        const pid_t child = fork();
        if (child < 0) {
            result.failure = std::strerror(errno);
            return result;
        }
        if (child == 0) {
            setpgid(0, 0);
            const char* input = spec.inputFile ? spec.inputFile->c_str() : "/dev/null";
            const int in = open(input, O_RDONLY);
            const int out = open(outFile.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
            const int err = open(errFile.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
            if (in < 0 || out < 0 || err < 0)
                _exit(126);
            dup2(in, STDIN_FILENO);
            dup2(out, STDOUT_FILENO);
            dup2(err, STDERR_FILENO);
            execvp(argv[0], argv.data());
            const char* message = "could not start program\n";
            (void)!write(STDERR_FILENO, message, std::strlen(message));
            _exit(127);
        }
        setpgid(child, child);
        result.started = true;
        waitFor(child, spec, result, &outFile, &errFile);
        result.output = readLimited(outFile, spec.outputLimit, result.outputLimited);
        result.errors = readLimited(errFile, spec.outputLimit, result.outputLimited);
        if (result.exitCode == 127 && result.errors == "could not start program\n") {
            result.started = false;
            result.failure = "'" + spec.program + "' was not found";
        }
        std::error_code ignored;
        fs::remove(outFile, ignored);
        fs::remove(errFile, ignored);
        return result;
    }

    ProcessResult runInteractive(const ProcessSpec& spec) override
    {
        ProcessResult result;
        std::vector<std::string> storage;
        auto argv = argumentVector(spec, storage);
        const pid_t child = fork();
        if (child < 0) {
            result.failure = std::strerror(errno);
            return result;
        }
        if (child == 0) {
            setpgid(0, 0);
            execvp(argv[0], argv.data());
            _exit(127);
        }
        setpgid(child, child);
        // Let the program read from the terminal while it runs.
        std::signal(SIGTTOU, SIG_IGN);
        tcsetpgrp(STDIN_FILENO, child);
        result.started = true;
        waitFor(child, spec, result, nullptr, nullptr);
        tcsetpgrp(STDIN_FILENO, getpgrp());
        return result;
    }
};

} // namespace

std::unique_ptr<IProcessRunner> createSystemProcessRunner()
{
    return std::make_unique<PosixProcessRunner>();
}

} // namespace ovc::platform

#endif // _WIN32
