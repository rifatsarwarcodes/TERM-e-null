#include "app/CommandLine.hpp"

#include "common/Strings.hpp"

namespace ovc::app {

CommandLine CommandLine::parse(const std::vector<std::string>& arguments)
{
    CommandLine line;
    bool fileGiven = false;
    for (std::size_t i = 0; i < arguments.size(); ++i) {
        const std::string& argument = arguments[i];
        auto nextValue = [&](const char* option) -> std::optional<std::string> {
            if (i + 1 >= arguments.size() || strings::startsWith(arguments[i + 1], "--")) {
                line.error = std::string(option) + " needs a value";
                return std::nullopt;
            }
            return arguments[++i];
        };

        if (argument == "--help" || argument == "-h") {
            line.mode = RunMode::Help;
        } else if (argument == "--version") {
            line.mode = RunMode::Version;
        } else if (argument == "--examiner") {
            line.mode = RunMode::Examiner;
        } else if (argument == "--setup") {
            line.mode = RunMode::Setup;
        } else if (argument == "--exam") {
            line.examPackage = nextValue("--exam");
        } else if (argument == "--ascii") {
            line.asciiBorders = true;
        } else if (strings::startsWith(argument, "-")) {
            line.error = "unknown option " + argument;
        } else if (!fileGiven) {
            line.file = argument;
            fileGiven = true;
        } else {
            line.error = "too many file names";
        }
        if (!line.error.empty())
            return line;
    }
    if (line.mode == RunMode::Setup && !fileGiven)
        line.file = "exam.ovx";
    if (line.mode == RunMode::Examiner && fileGiven)
        line.sessionPath = line.file;
    return line;
}

std::string CommandLine::usage()
{
    return "Usage:\n"
           "  ovc [file.cpp] [--exam exam.ovx]          start or resume an exam session (default main.cpp)\n"
           "  ovc --examiner [folder] [--exam exam.ovx] open the examiner dashboard\n"
           "  ovc --setup [exam.ovx]                    create an exam package\n"
           "  ovc --help | --version\n"
           "\n"
           "Options:\n"
           "  --ascii   draw borders with + - | for consoles without line-drawing characters\n";
}

} // namespace ovc::app
