#include "app/Launch.hpp"

#include "app/StudentApp.hpp"
#include "dashboard/ClassOverviewScreen.hpp"
#include "dashboard/Dashboard.hpp"
#include "dashboard/SessionLoader.hpp"
#include "dashboard/SessionScreen.hpp"
#include "platform/Console.hpp"
#include "common/Strings.hpp"
#include "common/Time.hpp"
#include "evidence/Seal.hpp"
#include "exam/PackageParser.hpp"
#include "exam/SetupWizard.hpp"
#include "platform/IProcessRunner.hpp"
#include "platform/ITerminal.hpp"

#include <filesystem>
#include <iostream>

namespace ovc::app {

namespace fs = std::filesystem;

namespace {

std::string askLine(const std::string& question)
{
    std::cout << question << std::flush;
    std::string answer;
    std::getline(std::cin, answer);
    return strings::trim(answer);
}

// Opens the dashboard for the running session when :dashboard is used at a viva.
class DashboardLauncher final : public IDashboardLauncher {
public:
    explicit DashboardLauncher(const exam::ExamPackage& package)
        : package_(package)
    {
    }

    void open(platform::ITerminal& terminal, const fs::path& sessionFolder) override
    {
        auto record = std::make_shared<const dashboard::SessionRecord>(
            dashboard::SessionLoader::load(sessionFolder, &package_));
        dashboard::Dashboard(std::make_unique<dashboard::SessionScreen>(record)).run(terminal);
    }

private:
    const exam::ExamPackage& package_;
};

bool askExaminerPasscode(const exam::ExamPackage& package)
{
    if (!package.passcode) {
        std::cout << "This exam package has no examiner passcode.\n";
        return true;
    }
    for (int attempt = 1; attempt <= 3; ++attempt) {
        std::cout << "Examiner passcode: " << std::flush;
        if (package.passcode->matches(platform::readSecretLine(std::cin, std::cout)))
            return true;
        std::cout << "Wrong passcode.\n";
    }
    return false;
}

} // namespace

std::optional<exam::ExamPackage> loadExamPackage(const CommandLine& commandLine, std::ostream& err)
{
    fs::path path;
    if (commandLine.examPackage)
        path = *commandLine.examPackage;
    else if (fs::exists("exam.ovx"))
        path = "exam.ovx";
    else
        return exam::ExamPackage{}; // practice session

    exam::ParseResult result = exam::loadPackage(path);
    if (!result.ok()) {
        err << "Problems in the exam package " << path.string() << ":\n";
        for (const auto& error : result.errors)
            err << "  " << error << "\n";
        return std::nullopt;
    }
    return result.package;
}

int runStudent(const CommandLine& commandLine)
{
    const auto package = loadExamPackage(commandLine, std::cerr);
    if (!package)
        return 1;

    std::cout << "OV_C exam environment\n";
    std::cout << package->title;
    if (package->timeLimitMinutes > 0)
        std::cout << "  (" << package->timeLimitMinutes << " minutes)";
    std::cout << "\n";
    if (package->isPractice())
        std::cout << "No exam package found: this is a practice session.\n";
    std::cout << "\n";

    std::string studentId;
    while (!session::isValidStudentId(studentId)) {
        studentId = askLine("Student ID: ");
        if (!std::cin)
            return 1;
        if (!session::isValidStudentId(studentId))
            std::cout << "  Use letters, digits, '-' or '_' only.\n";
    }

    session::SessionStore store(package->sessionsRoot, studentId);
    fs::path sourceFile = commandLine.file;
    session::SessionInfo identity;

    switch (store.state()) {
    case session::SessionState::Sealed: {
        const auto seal = evidence::Seal::read(store.sealFile());
        std::cout << "This session was already submitted";
        if (seal)
            std::cout << " (receipt " << seal->receipt << ")";
        std::cout << ". It cannot be opened again.\n";
        return 1;
    }
    case session::SessionState::Interrupted: {
        const auto info = store.readInfo();
        if (info) {
            std::cout << "Resuming the interrupted session of " << info->studentName << ".\n";
            sourceFile = info->fileName;
            identity = *info;
        }
        break;
    }
    case session::SessionState::New: {
        std::string name;
        while (name.empty()) {
            name = askLine("Full name: ");
            if (!std::cin)
                return 1;
        }
        identity.studentName = name;
        identity.studentId = studentId;
        identity.examCode = package->code;
        identity.examTitle = package->title;
        identity.fileName = sourceFile.filename().string();
        identity.startedAt = timeutil::nowEpochMillis();
        identity.timeLimit = static_cast<timeutil::Millis>(package->timeLimitMinutes) * 60'000;
        identity.packageHash = package->sourceHash;
        break;
    }
    }

    auto terminal = platform::createSystemTerminal();
    auto processes = platform::createSystemProcessRunner();
    DashboardLauncher launcher(*package);
    StudentApp app(*terminal, *processes, *package, store, identity, sourceFile, &launcher);
    if (const auto error = app.open()) {
        std::cerr << *error << "\n";
        return 1;
    }
    const int status = app.run();
    if (app.seal())
        std::cout << "Session sealed. Receipt code: " << app.seal()->receipt << "\n";
    return status;
}

int runExaminer(const CommandLine& commandLine)
{
    const auto package = loadExamPackage(commandLine, std::cerr);
    if (!package)
        return 1;
    if (!askExaminerPasscode(*package))
        return 1;

    const fs::path path = commandLine.sessionPath ? fs::path(*commandLine.sessionPath) : fs::path(package->sessionsRoot);
    if (!fs::exists(path)) {
        std::cerr << "No sessions found at " << path.string() << "\n";
        return 1;
    }
    std::unique_ptr<dashboard::Screen> root;
    if (fs::exists(path / "session.info"))
        root = std::make_unique<dashboard::SessionScreen>(
            std::make_shared<const dashboard::SessionRecord>(dashboard::SessionLoader::load(path, &*package)));
    else
        root = std::make_unique<dashboard::ClassOverviewScreen>(path, &*package);

    auto terminal = platform::createSystemTerminal();
    platform::TerminalGuard guard(*terminal);
    dashboard::Dashboard(std::move(root)).run(*terminal);
    return 0;
}

int runSetup(const CommandLine& commandLine)
{
    if (fs::exists(commandLine.file)) {
        const std::string answer = askLine(commandLine.file + " already exists. Replace it? (y/n): ");
        if (answer.empty() || (answer[0] != 'y' && answer[0] != 'Y'))
            return 1;
    }
    exam::SetupWizard wizard(std::cin, std::cout);
    return wizard.run(commandLine.file) ? 0 : 1;
}

} // namespace ovc::app
