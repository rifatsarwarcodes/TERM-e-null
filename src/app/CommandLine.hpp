#pragma once

#include <optional>
#include <string>
#include <vector>

namespace ovc::app {

enum class RunMode { Student, Examiner, Setup, Help, Version };

// ovc [file.cpp] [--exam exam.ovx]          student session (default main.cpp)
// ovc --examiner [session folder] [--exam]  examiner dashboard
// ovc --setup [exam.ovx]                    create an exam package
// ovc --help | --version
struct CommandLine {
    RunMode mode = RunMode::Student;
    std::string file = "main.cpp";     // student file, or package path for --setup
    std::optional<std::string> examPackage;
    std::optional<std::string> sessionPath; // examiner: one session or a folder of sessions
    bool asciiBorders = false;
    std::string error;                 // set when the arguments are wrong

    static CommandLine parse(const std::vector<std::string>& arguments);
    static std::string usage();
};

} // namespace ovc::app
