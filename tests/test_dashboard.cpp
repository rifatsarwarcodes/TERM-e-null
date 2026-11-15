#include "TestFramework.hpp"

#include "app/StudentApp.hpp"
#include "common/Files.hpp"
#include "dashboard/ClassOverviewScreen.hpp"
#include "dashboard/Dashboard.hpp"
#include "dashboard/MergeSort.hpp"
#include "dashboard/ReportWriter.hpp"
#include "dashboard/SessionLoader.hpp"
#include "dashboard/SessionScreen.hpp"
#include "dashboard/VivaFocus.hpp"
#include "platform/ScriptedTerminal.hpp"

#include <filesystem>

using namespace ovc;
using namespace ovc::dashboard;
using platform::KeyCode;
using platform::KeyEvent;
namespace fs = std::filesystem;

namespace {

struct PassingProcesses : platform::IProcessRunner {
    platform::ProcessResult run(const platform::ProcessSpec& spec) override
    {
        platform::ProcessResult result;
        result.started = true;
        result.exitCode = 0;
        if (spec.program != "g++")
            result.output = "3\n";
        return result;
    }
    platform::ProcessResult runInteractive(const platform::ProcessSpec& spec) override { return run(spec); }
};

const char* PastedCode =
    "int a, b;\n"
    "std::cin >> a >> b;\n"
    "std::cout << a + b << std::endl;\n"
    "return 0;\n"
    "// a long comment copied together with the code from a website\n"
    "// so that the paste is clearly larger than the small paste limit\n"
    "// and counts as a large outside paste for the rules of this exam\n"
    "// ......................................................................\n"
    "// ......................................................................\n";

// Records one complete session with the real student app and returns its folder.
fs::path recordSession(const fs::path& root, const std::string& id, exam::ExamPackage& package)
{
    package.title = "Sum";
    package.sessionsRoot = (root / "sessions").string();
    package.tests = {{"test 1", "1 2", "3"}};
    session::SessionInfo identity;
    identity.studentName = "Student " + id;
    identity.studentId = id;
    identity.fileName = "main.cpp";
    identity.startedAt = timeutil::nowEpochMillis();
    const fs::path source = root / (id + ".cpp");
    fs::remove(source);

    platform::ScriptedTerminal terminal({110, 32});
    PassingProcesses processes;
    session::SessionStore store(package.sessionsRoot, id);
    app::StudentApp app(terminal, processes, package, store, identity, source, nullptr, app::StudentAppOptions{false, 0});
    app.open();
    terminal.type("int main() {\n");
    terminal.queue({platform::InputEvent{platform::FocusEvent{false}}});
    terminal.queue({platform::InputEvent{platform::PasteEvent{PastedCode, true}}});
    terminal.press(KeyEvent::special(KeyCode::Escape));
    terminal.type(":test\n:q\ny");
    terminal.press(KeyEvent::special(KeyCode::Enter));
    terminal.press(KeyEvent::special(KeyCode::Enter));
    for (int i = 0; i < 500 && !app.finished(); ++i)
        app.step();
    return store.folder();
}

fs::path freshRoot(const std::string& name)
{
    const fs::path root = fs::temp_directory_path() / ("ovc_dash_" + name);
    fs::remove_all(root);
    fs::create_directories(root);
    return root;
}

} // namespace

TEST_CASE("merge sort: sorts and keeps equal items in order")
{
    std::vector<std::pair<int, char>> items{{3, 'a'}, {1, 'b'}, {3, 'c'}, {2, 'd'}, {1, 'e'}};
    mergeSort(items, [](const auto& x, const auto& y) { return x.first < y.first; });
    CHECK_EQ(items[0].second, 'b');
    CHECK_EQ(items[1].second, 'e');
    CHECK_EQ(items[3].second, 'a');
    CHECK_EQ(items[4].second, 'c');
}

TEST_CASE("viva focus: overlapping ranges with the same reason merge")
{
    const auto merged = mergeRanges({{3, 5, "paste", 80}, {5, 9, "paste", 60}, {20, 21, "paste", 60}, {4, 4, "test", 30}});
    CHECK_EQ(merged.size(), std::size_t(3));
    CHECK_EQ(merged[0].firstLine, std::size_t(3));
    CHECK_EQ(merged[0].lastLine, std::size_t(9));
    CHECK_EQ(merged[0].priority, 80);
}

TEST_CASE("loader: a sealed session verifies and is analysed")
{
    const fs::path root = freshRoot("sealed");
    exam::ExamPackage package;
    const fs::path folder = recordSession(root, "S1", package);
    const SessionRecord record = SessionLoader::load(folder, &package);

    CHECK_EQ(record.integrity.status(), std::string("SEALED"));
    CHECK(record.integrity.sealMatchesLogs);
    CHECK(record.integrity.submissionMatches && *record.integrity.submissionMatches);
    const auto seal = evidence::Seal::read(session::SessionStore(folder).sealFile());
    CHECK(seal && seal->receipt == record.integrity.receipt);

    CHECK_EQ(record.pastes.size(), std::size_t(1));
    CHECK_EQ(record.pastes[0].source, std::string("outside"));
    CHECK(record.pastes[0].retained > 0.99);
    CHECK(record.shares.outside > record.shares.typed);
    CHECK_EQ(record.testsPassed, 1);
    CHECK(record.totalPoints >= 6); // large paste + left unchanged
    CHECK(record.level != penalty::Level::Ok);
    CHECK(record.integrity.pointsMatchLog && *record.integrity.pointsMatchLog);
    CHECK(!record.focus.empty());
    CHECK(record.focus[0].reason.find("Outside paste") != std::string::npos);
    CHECK_EQ(record.replay.buffer.text(), files::read(root / "S1.cpp").value_or("?"));

    const std::string report = ReportWriter::sessionReport(record);
    CHECK(report.find(record.integrity.receipt) != std::string::npos);
    CHECK(report.find("Viva focus list") != std::string::npos);
}

TEST_CASE("loader: editing the event log is reported as altered")
{
    const fs::path root = freshRoot("tamper_log");
    exam::ExamPackage package;
    const fs::path folder = recordSession(root, "S2", package);
    const fs::path log = session::SessionStore(folder).eventsLog();
    std::string content = files::read(log).value_or("");
    const std::size_t at = content.find("src=outside");
    CHECK(at != std::string::npos);
    content.replace(at, 11, "src=self   ");
    files::writeAtomically(log, content);

    const SessionRecord record = SessionLoader::load(folder, &package);
    CHECK_EQ(record.integrity.status(), std::string("LOG ALTERED"));
    CHECK(record.integrity.eventsBrokenAt.has_value());
}

TEST_CASE("loader: a changed submission and a different package are noticed")
{
    const fs::path root = freshRoot("tamper_file");
    exam::ExamPackage package;
    const fs::path folder = recordSession(root, "S3", package);
    files::writeAtomically(session::SessionStore(folder).submissionDir() / "main.cpp", "int main() { return 0; }\n");

    exam::ExamPackage examinerCopy = package;
    examinerCopy.sourceFile = "exam.ovx";
    examinerCopy.sourceHash = std::string(64, 'f');
    const SessionRecord record = SessionLoader::load(folder, &examinerCopy);
    CHECK(record.integrity.submissionMatches && !*record.integrity.submissionMatches);
    CHECK(record.integrity.packageMatches && !*record.integrity.packageMatches);
}

TEST_CASE("dashboard: every screen can be opened and closed")
{
    const fs::path root = freshRoot("screens");
    exam::ExamPackage package;
    recordSession(root, "S4", package);
    recordSession(root, "S5", package);

    platform::ScriptedTerminal terminal({120, 36});
    auto overview = std::make_unique<ClassOverviewScreen>(root / "sessions", &package);
    CHECK_EQ(overview->rowCount(), std::size_t(2));
    Dashboard dashboard(std::move(overview));

    auto press = [&](KeyEvent key) {
        terminal.press(std::move(key));
        dashboard.step(terminal, 0);
    };
    auto type = [&](char c) { press(KeyEvent::character(std::string(1, c))); };

    dashboard.step(terminal, 0);
    type('s');                                        // sort by another column
    press(KeyEvent::special(KeyCode::Enter));         // open a session
    press(KeyEvent::special(KeyCode::Down));
    press(KeyEvent::special(KeyCode::Down));
    press(KeyEvent::special(KeyCode::Enter));         // event detail / saved output
    press(KeyEvent::special(KeyCode::Escape));
    type('g');                                        // origin view
    press(KeyEvent::special(KeyCode::Tab));
    press(KeyEvent::special(KeyCode::Down));
    press(KeyEvent::special(KeyCode::Escape));
    type('v');                                        // viva focus list
    press(KeyEvent::special(KeyCode::Escape));
    type('r');                                        // replay
    type('n');
    type('n');
    press(KeyEvent::special(KeyCode::Left));
    press(KeyEvent::special(KeyCode::End));
    press(KeyEvent::special(KeyCode::Escape));
    type('o');                                        // all outputs
    press(KeyEvent::special(KeyCode::PageDown));
    press(KeyEvent::special(KeyCode::Escape));
    type('k');                                        // check the receipt
    type('0');
    press(KeyEvent::special(KeyCode::Enter));
    press(KeyEvent::special(KeyCode::Escape));        // (drawn first) back to overview
    CHECK(terminal.output().find("DOES NOT MATCH") != std::string::npos);
    CHECK(!dashboard.done());
    type('q');
    CHECK(dashboard.done());
    CHECK(terminal.output().find("CLASS OVERVIEW") != std::string::npos);
    CHECK(terminal.output().find("REPLAY") != std::string::npos);
    CHECK(terminal.output().find("VIVA FOCUS") != std::string::npos);
}
