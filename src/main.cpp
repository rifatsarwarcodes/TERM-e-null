#include "app/CommandLine.hpp"
#include "app/Launch.hpp"
#include "ui/ScreenBuffer.hpp"

#include <exception>
#include <iostream>
#include <string>
#include <vector>

namespace {

constexpr const char* Version = "ovc 1.0.0";

} // namespace

int main(int argc, char** argv)
{
    using namespace ovc::app;

    const CommandLine commandLine = CommandLine::parse(std::vector<std::string>(argv + 1, argv + argc));
    if (!commandLine.error.empty()) {
        std::cerr << "ovc: " << commandLine.error << "\n\n" << CommandLine::usage();
        return 2;
    }
    ovc::ui::ScreenBuffer::setAsciiBorders(commandLine.asciiBorders);

    try {
        switch (commandLine.mode) {
        case RunMode::Help: std::cout << CommandLine::usage(); return 0;
        case RunMode::Version: std::cout << Version << "\n"; return 0;
        case RunMode::Setup: return runSetup(commandLine);
        case RunMode::Examiner: return runExaminer(commandLine);
        case RunMode::Student: return runStudent(commandLine);
        }
    } catch (const std::exception& error) {
        std::cerr << "ovc: " << error.what() << "\n";
        return 1;
    }
    return 0;
}
