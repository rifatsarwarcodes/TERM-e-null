// Process runner for Windows: CreateProcess with output redirected to
// temporary files and a Job Object so a runaway program and everything it
// started can be stopped together.
#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "common/Files.hpp"
#include "platform/IProcessRunner.hpp"

namespace ovc::platform {

namespace {

namespace fs = std::filesystem;

// Quotes one argument following the rules of CommandLineToArgvW.
std::string quoteArgument(const std::string& argument)
{
    if (!argument.empty() && argument.find_first_of(" \t\"") == std::string::npos)
        return argument;
    std::string quoted = "\"";
    std::size_t backslashes = 0;
    for (char c : argument) {
        if (c == '\\') {
            ++backslashes;
        } else if (c == '"') {
            quoted.append(backslashes * 2 + 1, '\\');
            quoted.push_back('"');
            backslashes = 0;
            continue;
        } else {
            backslashes = 0;
        }
        quoted.push_back(c);
    }
    quoted.append(backslashes, '\\');
    quoted.push_back('"');
    return quoted;
}

std::string commandLine(const ProcessSpec& spec)
{
    std::string line = quoteArgument(spec.program);
    for (const auto& argument : spec.arguments)
        line += " " + quoteArgument(argument);
    return line;
}

fs::path temporaryFile(const char* tag)
{
    static int counter = 0;
    return fs::temp_directory_path()
        / ("ovc_" + std::to_string(GetCurrentProcessId()) + "_" + std::to_string(++counter) + "_" + tag + ".txt");
}

HANDLE openInheritable(const fs::path& path, bool write)
{
    SECURITY_ATTRIBUTES security{};
    security.nLength = sizeof security;
    security.bInheritHandle = TRUE;
    return CreateFileW(path.wstring().c_str(), write ? GENERIC_WRITE : GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE, &security, write ? CREATE_ALWAYS : OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, nullptr);
}

HANDLE openNulInput()
{
    SECURITY_ATTRIBUTES security{};
    security.nLength = sizeof security;
    security.bInheritHandle = TRUE;
    return CreateFileA("NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &security, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, nullptr);
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

std::string lastErrorText()
{
    return "Windows error " + std::to_string(GetLastError());
}

class WinProcessRunner final : public IProcessRunner {
public:
    ProcessResult run(const ProcessSpec& spec) override
    {
        const fs::path outFile = temporaryFile("out");
        const fs::path errFile = temporaryFile("err");
        HANDLE in = spec.inputFile ? openInheritable(*spec.inputFile, false) : openNulInput();
        HANDLE out = openInheritable(outFile, true);
        HANDLE err = openInheritable(errFile, true);

        ProcessResult result = start(spec, in, out, err, true, &outFile, &errFile);

        CloseHandle(in);
        CloseHandle(out);
        CloseHandle(err);
        if (result.started) {
            result.output = readLimited(outFile, spec.outputLimit, result.outputLimited);
            result.errors = readLimited(errFile, spec.outputLimit, result.outputLimited);
        }
        std::error_code ignored;
        fs::remove(outFile, ignored);
        fs::remove(errFile, ignored);
        return result;
    }

    ProcessResult runInteractive(const ProcessSpec& spec) override
    {
        return start(spec, nullptr, nullptr, nullptr, false, nullptr, nullptr);
    }

private:
    ProcessResult start(const ProcessSpec& spec, HANDLE in, HANDLE out, HANDLE err, bool redirect,
        const fs::path* outFile, const fs::path* errFile)
    {
        ProcessResult result;
        STARTUPINFOA startup{};
        startup.cb = sizeof startup;
        if (redirect) {
            if (in == INVALID_HANDLE_VALUE || out == INVALID_HANDLE_VALUE || err == INVALID_HANDLE_VALUE) {
                result.failure = "could not open input or output files";
                return result;
            }
            startup.dwFlags = STARTF_USESTDHANDLES;
            startup.hStdInput = in;
            startup.hStdOutput = out;
            startup.hStdError = err;
        }

        HANDLE job = CreateJobObjectA(nullptr, nullptr);
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (job != nullptr)
            SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof limits);

        std::string line = commandLine(spec);
        PROCESS_INFORMATION process{};
        const BOOL created = CreateProcessA(nullptr, line.data(), nullptr, nullptr, redirect ? TRUE : FALSE,
            CREATE_SUSPENDED, nullptr, nullptr, &startup, &process);
        if (!created) {
            result.failure = "'" + spec.program + "' could not be started (" + lastErrorText() + ")";
            if (job != nullptr)
                CloseHandle(job);
            return result;
        }
        if (job != nullptr)
            AssignProcessToJobObject(job, process.hProcess);
        ResumeThread(process.hThread);
        result.started = true;

        const ULONGLONG begin = GetTickCount64();
        while (WaitForSingleObject(process.hProcess, 20) == WAIT_TIMEOUT) {
            const auto elapsed = static_cast<long long>(GetTickCount64() - begin);
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
                if (job != nullptr)
                    TerminateJobObject(job, 1);
                else
                    TerminateProcess(process.hProcess, 1);
                WaitForSingleObject(process.hProcess, INFINITE);
                break;
            }
        }
        result.duration = static_cast<timeutil::Millis>(GetTickCount64() - begin);
        DWORD exitCode = 0;
        GetExitCodeProcess(process.hProcess, &exitCode);
        result.exitCode = static_cast<int>(exitCode);

        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        if (job != nullptr)
            CloseHandle(job);
        return result;
    }
};

} // namespace

std::unique_ptr<IProcessRunner> createSystemProcessRunner()
{
    return std::make_unique<WinProcessRunner>();
}

} // namespace ovc::platform

#endif // _WIN32
