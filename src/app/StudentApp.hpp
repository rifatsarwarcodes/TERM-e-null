#pragma once

#include "analysis/BurstDetector.hpp"
#include "app/StudentNotifier.hpp"
#include "app/StudentScreen.hpp"
#include "app/StudentSession.hpp"
#include "build/BuildRunner.hpp"
#include "editor/Editor.hpp"
#include "platform/IProcessRunner.hpp"
#include "platform/ITerminal.hpp"
#include "ui/AnsiRenderer.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace ovc::app {

// Opens the examiner dashboard for a session folder (provided by main, so the
// student side does not depend on the dashboard code).
class IDashboardLauncher {
public:
    virtual ~IDashboardLauncher() = default;
    virtual void open(platform::ITerminal& terminal, const std::filesystem::path& sessionFolder) = 0;
};

struct StudentAppOptions {
    bool waitAfterInteractiveRun = true; // "press Enter to return" after :run
    int inputTimeoutMs = 200;
};

// The student's side of OV_C: reads input, drives the editor, handles the
// exam commands (save, compile, run, test, rules, notes, dashboard, quit),
// watches the clock and seals the session at the end.
class StudentApp final : public editor::IEditorHost {
public:
    StudentApp(platform::ITerminal& terminal, platform::IProcessRunner& processes, const exam::ExamPackage& package,
        session::SessionStore store, session::SessionInfo identity, std::filesystem::path sourceFile,
        IDashboardLauncher* dashboard = nullptr, StudentAppOptions options = {});

    // Opens or resumes the session. Returns an error message on failure.
    std::optional<std::string> open();
    // Main loop until the student has closed the receipt.
    int run();
    // One loop iteration: check the clock, draw, read and handle input.
    void step();

    bool finished() const { return finished_; }
    bool ended() const { return ended_; }
    const editor::Editor& editor() const { return editor_; }
    const std::string& notice() const { return notice_; }
    const std::optional<evidence::Seal>& seal() const { return seal_; }
    const StudentSession& session() const { return session_; }

    // editor::IEditorHost
    void onExCommand(const std::string& name, const std::string& argument) override;
    editor::Origin onInsidePaste(const std::string& text) override;

private:
    void handle(const platform::InputEvent& event);
    void handleKey(const platform::KeyEvent& key);
    bool handleAppKey(const platform::KeyEvent& key);
    void handleOverlayKey(const platform::KeyEvent& key);
    void handlePaste(const std::string& text, bool bracketed);
    void handleFocus(bool gained);

    void save();
    bool compile();
    void runProgram(const std::string& inputName);
    void runTests();
    void jumpToNextError();
    void askForNote();
    void askToEnd();
    void openDashboard();
    void endSession(const std::string& reason);

    void showOverlay(std::string title, std::vector<std::string> lines);
    void showRules();
    void showProblem();
    void showHelp();
    void setNotice(std::string text);
    void checkClock();
    void draw();

    std::string fileName() const;
    std::filesystem::path executable() const;

    platform::ITerminal& terminal_;
    const exam::ExamPackage& package_;
    session::SessionInfo identity_;
    std::filesystem::path sourceFile_;
    IDashboardLauncher* dashboard_;
    StudentAppOptions options_;

    editor::Editor editor_;
    StudentSession session_;
    build::BuildRunner builder_;
    StudentNotifier notifier_;
    analysis::BurstDetector bursts_;
    StudentScreen screen_;
    ui::ScreenBuffer frame_;
    ui::AnsiRenderer renderer_;

    ResultPanel panel_;
    std::optional<Overlay> overlay_;
    std::vector<build::CompilerMessage> errors_;
    std::size_t nextError_ = 0;
    std::string lastRunOutput_;
    std::string notice_;
    timeutil::Millis noticeSince_ = 0;
    std::string diskHash_;
    std::optional<timeutil::Millis> focusLostAt_;
    int passcodeFailures_ = 0;
    bool warnedTen_ = false;
    bool warnedTwo_ = false;
    bool showProblem_ = true;
    bool ended_ = false;
    bool finished_ = false;
    std::optional<evidence::Seal> seal_;
};

} // namespace ovc::app
