#include "TestFramework.hpp"

#include "app/CommandLine.hpp"
#include "app/StudentApp.hpp"
#include "common/Files.hpp"
#include "evidence/HashChain.hpp"
#include "evidence/Seal.hpp"
#include "platform/ScriptedTerminal.hpp"

#include <filesystem>
#include <memory>

using namespace ovc;
using namespace ovc::app;
using platform::KeyCode;
using platform::KeyEvent;
namespace fs = std::filesystem;

namespace {

// Compiles "successfully" and answers every test with "3".
struct FakeProcesses : platform::IProcessRunner {
    int compiles = 0;
    platform::ProcessResult run(const platform::ProcessSpec& spec) override
    {
        platform::ProcessResult result;
        result.started = true;
        result.exitCode = 0;
        if (spec.program == "g++")
            ++compiles;
        else
            result.output = "3\n";
        return result;
    }
    platform::ProcessResult runInteractive(const platform::ProcessSpec& spec) override { return run(spec); }
};

struct Fixture {
    fs::path root;
    fs::path source;
    exam::ExamPackage package;
    session::SessionInfo identity;

    explicit Fixture(const std::string& name)
    {
        root = fs::temp_directory_path() / ("ovc_app_" + name);
        fs::remove_all(root);
        fs::create_directories(root);
        source = root / "main.cpp";
        package.title = "Sum";
        package.sessionsRoot = (root / "sessions").string();
        package.tests.push_back({"test 1", "1 2", "3"});
        identity.studentName = "Student A";
        identity.studentId = "240042125";
        identity.fileName = "main.cpp";
        identity.startedAt = timeutil::nowEpochMillis();
    }

    session::SessionStore store() const { return session::SessionStore(package.sessionsRoot, identity.studentId); }
};

void drain(StudentApp& app, platform::ScriptedTerminal& terminal)
{
    int guard = 0;
    while ((terminal.hasInput() || guard == 0) && !app.finished() && guard < 1000) {
        app.step();
        ++guard;
    }
}

void command(platform::ScriptedTerminal& terminal, const std::string& text)
{
    terminal.press(KeyEvent::special(KeyCode::Escape));
    terminal.type(":" + text + "\n");
}

std::vector<std::string> eventTypes(const fs::path& log)
{
    std::vector<std::string> types;
    for (const auto& body : evidence::readChain(log).bodies) {
        const auto event = session::Event::decode(body);
        types.push_back(event ? session::eventTypeName(event->type()) : "?");
    }
    return types;
}

bool contains(const std::vector<std::string>& items, const std::string& item)
{
    for (const auto& value : items)
        if (value == item)
            return true;
    return false;
}

} // namespace

TEST_CASE("command line: modes and options")
{
    CHECK(CommandLine::parse({}).mode == RunMode::Student);
    CHECK_EQ(CommandLine::parse({"sum.cpp"}).file, std::string("sum.cpp"));
    const CommandLine examiner = CommandLine::parse({"--examiner", "sessions/LT2", "--exam", "lab.ovx"});
    CHECK(examiner.mode == RunMode::Examiner);
    CHECK(examiner.sessionPath && *examiner.sessionPath == "sessions/LT2");
    CHECK(examiner.examPackage && *examiner.examPackage == "lab.ovx");
    CHECK_EQ(CommandLine::parse({"--setup"}).file, std::string("exam.ovx"));
    CHECK(!CommandLine::parse({"--exam"}).error.empty());
    CHECK(!CommandLine::parse({"--bogus"}).error.empty());
}

TEST_CASE("student app: a whole session from start to sealed receipt")
{
    Fixture fixture("full");
    files::writeAtomically(fixture.source, "// sum\n");
    platform::ScriptedTerminal terminal({110, 32});
    FakeProcesses processes;
    StudentApp app(terminal, processes, fixture.package, fixture.store(), fixture.identity, fixture.source, nullptr,
        StudentAppOptions{false, 0});
    CHECK(!app.open().has_value());

    terminal.type("int main() {\n");
    terminal.queue({platform::InputEvent{platform::PasteEvent{
        "int a, b;\nstd::cin >> a >> b;\nstd::cout << a + b << std::endl;\nreturn 0;\n// copied from somewhere\n",
        true}}});
    terminal.press(KeyEvent::control('n'));
    terminal.type("from my lab 1\n");
    command(terminal, "test");
    command(terminal, "q");
    terminal.type("y");
    terminal.press(KeyEvent::special(KeyCode::Enter));
    terminal.press(KeyEvent::special(KeyCode::Enter)); // close the receipt
    drain(app, terminal);

    CHECK(app.finished());
    CHECK(app.seal().has_value());
    CHECK(processes.compiles >= 1);

    const auto store = fixture.store();
    CHECK(store.state() == session::SessionState::Sealed);
    const auto types = eventTypes(store.eventsLog());
    CHECK_EQ(types.front(), std::string("SESSION_START"));
    CHECK(contains(types, "PASTE"));
    CHECK(contains(types, "PASTE_NOTE"));
    CHECK(contains(types, "TEST"));
    CHECK(contains(types, "SESSION_END"));

    const auto events = evidence::readChain(store.eventsLog());
    const auto journal = evidence::readChain(store.journalLog());
    CHECK(events.intact);
    CHECK(journal.intact);
    CHECK_EQ(app.seal()->receipt, evidence::receiptCode(events.headHash, journal.headHash));

    // The saved file and the submission match what was typed and pasted.
    const std::string saved = files::read(fixture.source).value_or("");
    CHECK(saved.find("std::cin >> a >> b;") != std::string::npos);
    CHECK_EQ(files::read(store.submissionDir() / "main.cpp").value_or(""), saved);
    CHECK(fs::exists(store.pasteFile(4)) || fs::exists(store.pastesDir()));
}

TEST_CASE("student app: an interrupted session resumes with the same code")
{
    Fixture fixture("resume");
    {
        platform::ScriptedTerminal terminal;
        FakeProcesses processes;
        StudentApp app(terminal, processes, fixture.package, fixture.store(), fixture.identity, fixture.source,
            nullptr, StudentAppOptions{false, 0});
        CHECK(!app.open().has_value());
        terminal.type("int x = 1;\nint y = 2;");
        terminal.press(KeyEvent::control('z'));
        drain(app, terminal);
        // No :q: the program "crashes" here.
    }
    CHECK(fixture.store().state() == session::SessionState::Interrupted);

    platform::ScriptedTerminal terminal;
    FakeProcesses processes;
    StudentApp app(terminal, processes, fixture.package, fixture.store(), fixture.identity, fixture.source, nullptr,
        StudentAppOptions{false, 0});
    CHECK(!app.open().has_value());
    CHECK(app.session().resumed());
    CHECK_EQ(app.editor().buffer().text(), std::string("int x = 1;\n"));
    const auto types = eventTypes(fixture.store().eventsLog());
    CHECK(contains(types, "SESSION_RESUMED"));
    CHECK(evidence::readChain(fixture.store().eventsLog()).intact);
}

TEST_CASE("student app: a sealed session cannot be opened again")
{
    Fixture fixture("sealed");
    fs::create_directories(fixture.store().folder());
    fixture.store().writeInfo(fixture.identity);
    files::writeAtomically(fixture.store().sealFile(), "receipt=AAAA-BBBB-CC\n");
    platform::ScriptedTerminal terminal;
    FakeProcesses processes;
    StudentApp app(terminal, processes, fixture.package, fixture.store(), fixture.identity, fixture.source);
    CHECK(app.open().has_value());
}

TEST_CASE("student app: focus changes are recorded with time away")
{
    Fixture fixture("focus");
    platform::ScriptedTerminal terminal;
    FakeProcesses processes;
    StudentApp app(terminal, processes, fixture.package, fixture.store(), fixture.identity, fixture.source, nullptr,
        StudentAppOptions{false, 0});
    CHECK(!app.open().has_value());
    terminal.queue({platform::InputEvent{platform::FocusEvent{false}}});
    terminal.queue({platform::InputEvent{platform::FocusEvent{true}}});
    drain(app, terminal);
    const auto types = eventTypes(fixture.store().eventsLog());
    CHECK(contains(types, "FOCUS_LOST"));
    CHECK(contains(types, "FOCUS_GAINED"));
}
