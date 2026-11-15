#include "app/StudentApp.hpp"

#include "common/Files.hpp"
#include "common/Strings.hpp"
#include "crypto/Sha256.hpp"
#include "penalty/PenaltyEngine.hpp"
#include "ui/Theme.hpp"

#include <iostream>
#include <variant>

namespace ovc::app {

using platform::InputEvent;
using platform::KeyCode;
using platform::KeyEvent;
using session::Event;
using session::EventType;
namespace theme = ui::theme;

namespace {

constexpr timeutil::Millis NoticeLifetime = 20'000;

std::string lineCountText(const std::string& text)
{
    const std::size_t lines = strings::countLines(text);
    return std::to_string(lines) + (lines == 1 ? " line" : " lines");
}

} // namespace

StudentApp::StudentApp(platform::ITerminal& terminal, platform::IProcessRunner& processes,
    const exam::ExamPackage& package, session::SessionStore store, session::SessionInfo identity,
    std::filesystem::path sourceFile, IDashboardLauncher* dashboard, StudentAppOptions options)
    : terminal_(terminal)
    , package_(package)
    , identity_(std::move(identity))
    , sourceFile_(std::move(sourceFile))
    , dashboard_(dashboard)
    , options_(options)
    , session_(package, std::move(store))
    , builder_(processes, build::BuildSettings::fromPackage(package))
    , notifier_([this](std::string text) { setNotice(std::move(text)); })
{
    editor_.setHost(this);
}

std::optional<std::string> StudentApp::open()
{
    if (auto error = session_.open(editor_, identity_, sourceFile_))
        return error;
    session_.subscribe(&notifier_);
    diskHash_ = crypto::Sha256::hex(files::read(sourceFile_).value_or(""));
    if (session_.resumed())
        setNotice("Session resumed. Your code was restored from the journal.");
    if (session_.exam().clock().timeUp())
        endSession("time");
    return std::nullopt;
}

int StudentApp::run()
{
    platform::TerminalGuard guard(terminal_);
    renderer_.invalidate();
    while (!finished_)
        step();
    return 0;
}

void StudentApp::step()
{
    if (terminal_.closeRequested() && !ended_) {
        endSession("window closed");
        terminal_.shutdownComplete();
        finished_ = true;
        return;
    }
    checkClock();
    draw();
    for (const auto& event : bursts_.process(terminal_.read(options_.inputTimeoutMs))) {
        handle(event);
        if (finished_)
            break;
    }
}

// ------------------------------------------------------------------ input

void StudentApp::handle(const InputEvent& event)
{
    if (const auto* key = std::get_if<KeyEvent>(&event))
        handleKey(*key);
    else if (const auto* paste = std::get_if<platform::PasteEvent>(&event))
        handlePaste(paste->text, paste->bracketed);
    else if (const auto* focus = std::get_if<platform::FocusEvent>(&event))
        handleFocus(focus->gained);
    else
        renderer_.invalidate(); // resize
}

void StudentApp::handleKey(const KeyEvent& key)
{
    if (overlay_) {
        handleOverlayKey(key);
        return;
    }
    if (!editor_.promptActive() && handleAppKey(key))
        return;
    editor_.handleKey(key);
}

bool StudentApp::handleAppKey(const KeyEvent& key)
{
    if (key.is(KeyCode::F1)) showHelp();
    else if (key.is(KeyCode::F2)) showProblem_ = !showProblem_;
    else if (key.is(KeyCode::F5)) compile();
    else if (key.is(KeyCode::F6)) runProgram("");
    else if (key.is(KeyCode::F7)) runTests();
    else if (key.is(KeyCode::F8)) jumpToNextError();
    else if (key.isCtrl('n')) askForNote();
    else if (key.isCtrl('q')) askToEnd();
    else return false;
    return true;
}

void StudentApp::handleOverlayKey(const KeyEvent& key)
{
    Overlay& overlay = *overlay_;
    const std::size_t page = static_cast<std::size_t>(screen_.overlayRows());
    if (key.is(KeyCode::Up) && overlay.scroll > 0) {
        --overlay.scroll;
    } else if (key.is(KeyCode::Down)) {
        ++overlay.scroll;
    } else if (key.is(KeyCode::PageUp)) {
        overlay.scroll = overlay.scroll > page ? overlay.scroll - page : 0;
    } else if (key.is(KeyCode::PageDown)) {
        overlay.scroll += page;
    } else if (overlay.endsProgram) {
        if (key.is(KeyCode::Enter))
            finished_ = true;
    } else if (key.is(KeyCode::Escape) || key.is(KeyCode::Enter) || key.isChar('q')) {
        overlay_.reset();
    }
    if (overlay_ && overlay_->scroll + 1 > overlay_->lines.size())
        overlay_->scroll = overlay_->lines.empty() ? 0 : overlay_->lines.size() - 1;
}

void StudentApp::handlePaste(const std::string& text, bool bracketed)
{
    if (overlay_ || text.empty())
        return;
    if (editor_.promptActive()) {
        editor_.appendToPrompt(text);
        return;
    }
    if (editor_.readOnly())
        return;

    const analysis::PasteSource source = session_.classify(text, editor_.buffer());
    const std::uint64_t id = session_.nextSequence();
    const editor::Origin origin = editor::Origin::paste(analysis::originKindFor(source), static_cast<int>(id));
    editor_.insertPaste(text, origin);

    const auto file = session_.store().pasteFile(id);
    files::writeAtomically(file, text);
    Event event(EventType::Paste);
    event.set("src", std::string(analysis::pasteSourceName(source)))
        .set("conf", std::string(bracketed ? "confirmed" : "probable"))
        .set("chars", static_cast<long long>(text.size()))
        .set("lines", static_cast<long long>(strings::countLines(text)))
        .set("file", "pastes/" + file.filename().string())
        .set("sha", crypto::Sha256::hex(text));
    session_.publish(event);
    session_.rememberPaste({id, origin, text.size(), analysis::pasteSourceName(source)});

    const penalty::PenaltyPolicy& policy = package_.policy;
    const bool large = static_cast<int>(text.size()) > policy.smallPasteChars
        || static_cast<int>(strings::countLines(text)) > policy.smallPasteLines;
    if (source == analysis::PasteSource::Outside && large)
        setNotice("Paste recorded (" + lineCountText(text) + ", from outside the editor). Press Ctrl+N to add a note.");
    else if (source == analysis::PasteSource::Self)
        editor_.setMessage("Paste recorded (your own code).");
    else if (source == analysis::PasteSource::Allowed)
        editor_.setMessage("Paste recorded (allowed snippet).");
}

editor::Origin StudentApp::onInsidePaste(const std::string& text)
{
    const std::uint64_t id = session_.nextSequence();
    const editor::Origin origin = editor::Origin::paste(editor::OriginKind::Inside, static_cast<int>(id));
    Event event(EventType::Paste);
    event.set("src", std::string("inside"))
        .set("conf", std::string("certain"))
        .set("chars", static_cast<long long>(text.size()))
        .set("lines", static_cast<long long>(strings::countLines(text)));
    session_.publish(event);
    session_.rememberPaste({id, origin, text.size(), "inside"});
    return origin;
}

void StudentApp::handleFocus(bool gained)
{
    if (ended_)
        return;
    const timeutil::Millis now = session_.exam().clock().now();
    if (!gained) {
        focusLostAt_ = now;
        session_.publish(Event(EventType::FocusLost));
        return;
    }
    Event event(EventType::FocusGained);
    if (focusLostAt_)
        event.set("away_ms", static_cast<long long>(now - *focusLostAt_));
    focusLostAt_.reset();
    session_.publish(event);
}

// ------------------------------------------------------------------ commands

void StudentApp::onExCommand(const std::string& name, const std::string& argument)
{
    if (name == "w") save();
    else if (name == "q" || name == "q!" || name == "wq" || name == "x") askToEnd();
    else if (name == "compile" || name == "make") compile();
    else if (name == "run") runProgram(argument);
    else if (name == "test") runTests();
    else if (name == "rules") showRules();
    else if (name == "problem") showProblem();
    else if (name == "help") showHelp();
    else if (name == "cn" || name == "next") jumpToNextError();
    else if (name == "output") showOverlay("LAST PROGRAM OUTPUT", strings::splitLines(lastRunOutput_.empty() ? "(nothing has been run yet)" : lastRunOutput_));
    else if (name == "note") {
        const auto paste = session_.lastPaste();
        if (!paste || argument.empty()) {
            askForNote();
        } else {
            Event note(EventType::PasteNote);
            note.set("paste", static_cast<long long>(paste->id)).set("text", argument);
            session_.publish(note);
            editor_.setMessage("Note saved for paste #" + std::to_string(paste->id) + ".");
        }
    } else if (name == "dashboard") openDashboard();
    else editor_.setMessage("Unknown command :" + name + "  (F1 shows all commands)");
}

void StudentApp::save()
{
    if (ended_)
        return;
    const auto onDisk = files::read(sourceFile_);
    if (onDisk && crypto::Sha256::hex(*onDisk) != diskHash_) {
        Event changed(EventType::FileChangedOutside);
        changed.set("when", std::string("during session"))
            .set("disk_hash", crypto::Sha256::hex(*onDisk))
            .set("expected_hash", diskHash_);
        session_.publish(changed);
        setNotice("The file was changed by another program. This was recorded; your editor version is kept.");
    }
    const std::string text = editor_.buffer().text();
    if (!files::writeAtomically(sourceFile_, text)) {
        editor_.setMessage("Could not save " + fileName() + ".");
        return;
    }
    diskHash_ = crypto::Sha256::hex(text);
    editor_.markSaved();
    Event saved(EventType::Save);
    saved.set("hash", diskHash_)
        .set("bytes", static_cast<long long>(text.size()))
        .set("lines", static_cast<long long>(editor_.buffer().lineCount()));
    session_.publish(saved);
    editor_.setMessage("Saved " + fileName() + " (" + std::to_string(editor_.buffer().lineCount()) + " lines).");
}

bool StudentApp::compile()
{
    if (ended_)
        return false;
    save();
    const std::uint64_t id = session_.nextSequence();
    const build::CompileResult result = builder_.compile(sourceFile_, executable());
    files::writeAtomically(session_.store().runFile(id), result.rawOutput);

    Event event(EventType::Compile);
    event.set("ok", static_cast<long long>(result.ok))
        .set("errors", static_cast<long long>(result.errorCount()))
        .set("warnings", static_cast<long long>(result.warningCount()))
        .set("ms", static_cast<long long>(result.process.duration))
        .set("file", "runs/" + session_.store().runFile(id).filename().string());
    session_.publish(event);

    errors_.clear();
    for (const auto& message : result.messages)
        if (message.severity != build::Severity::Note)
            errors_.push_back(message);
    nextError_ = 0;

    panel_ = ResultPanel{};
    if (result.compilerMissing) {
        panel_.title = "COMPILE";
        panel_.lines.emplace_back("The compiler '" + package_.compiler + "' was not found. Is it on the PATH?", theme::bad());
        return false;
    }
    if (result.ok) {
        panel_.title = "COMPILE";
        std::string text = "Compiled successfully";
        if (result.warningCount() > 0)
            text += " with " + std::to_string(result.warningCount()) + " warning(s)  (F8 to see them)";
        panel_.lines.emplace_back(text, result.warningCount() > 0 ? theme::warn() : theme::ok());
        return true;
    }
    panel_.title = "COMPILE ERRORS  (F8 next error)";
    for (const auto& message : errors_)
        panel_.lines.emplace_back(std::to_string(message.line) + ":" + std::to_string(message.column) + " "
                + build::severityName(message.severity) + ": " + message.text,
            message.severity == build::Severity::Error ? theme::bad() : theme::warn());
    if (errors_.empty())
        panel_.lines.emplace_back(strings::trim(result.rawOutput).substr(0, 200), theme::bad());
    jumpToNextError();
    return false;
}

void StudentApp::runProgram(const std::string& inputName)
{
    if (ended_ || !compile())
        return;
    const std::uint64_t id = session_.nextSequence();
    Event event(EventType::Run);

    if (!inputName.empty()) {
        const std::filesystem::path input = sourceFile_.parent_path() / inputName;
        if (!std::filesystem::exists(input)) {
            editor_.setMessage("Input file not found: " + inputName);
            return;
        }
        const platform::ProcessResult result = builder_.run(executable(), input);
        lastRunOutput_ = result.output + (result.errors.empty() ? "" : "\n--- errors ---\n" + result.errors);
        files::writeAtomically(session_.store().runFile(id),
            "input: " + inputName + "\nexit: " + std::to_string(result.exitCode) + "\n--- output ---\n" + lastRunOutput_);
        event.set("mode", std::string("file"))
            .set("input", inputName)
            .set("exit", static_cast<long long>(result.exitCode))
            .set("ms", static_cast<long long>(result.duration))
            .set("timed_out", static_cast<long long>(result.timedOut))
            .set("output_limited", static_cast<long long>(result.outputLimited))
            .set("file", "runs/" + session_.store().runFile(id).filename().string());
        session_.publish(event);

        panel_ = ResultPanel{};
        panel_.title = "RUN  (input " + inputName + ", :output shows everything)";
        if (result.timedOut)
            panel_.lines.emplace_back("Stopped: the program ran longer than " + std::to_string(package_.runTimeLimitSeconds) + " s.", theme::bad());
        for (const auto& line : strings::splitLines(result.output))
            panel_.lines.emplace_back(line, theme::text());
        if (panel_.lines.empty())
            panel_.lines.emplace_back("(no output)", theme::dim());
        panel_.lines.emplace_back("exit code " + std::to_string(result.exitCode), result.exitCode == 0 ? theme::ok() : theme::warn());
        return;
    }

    // Interactive: give the terminal to the program.
    terminal_.stop();
    std::cout << "\n--- Running " << fileName() << ". Type the input yourself. "
              << "This output is not recorded; use :run <file> or :test for recorded runs. ---\n\n" << std::flush;
    const platform::ProcessResult result = builder_.runInteractive(executable());
    std::cout << "\n--- Program ended with exit code " << result.exitCode
              << (result.timedOut ? " (stopped: time limit)" : "") << ". ";
    if (options_.waitAfterInteractiveRun) {
        std::cout << "Press Enter to return to the editor. ---" << std::flush;
        std::string ignored;
        std::getline(std::cin, ignored);
    }
    terminal_.start();
    renderer_.invalidate();

    event.set("mode", std::string("interactive"))
        .set("exit", static_cast<long long>(result.exitCode))
        .set("ms", static_cast<long long>(result.duration))
        .set("timed_out", static_cast<long long>(result.timedOut));
    session_.publish(event);
    editor_.setMessage("Program ended with exit code " + std::to_string(result.exitCode) + ".");
}

void StudentApp::runTests()
{
    if (ended_)
        return;
    if (package_.tests.empty()) {
        editor_.setMessage("This exam has no sample tests.");
        return;
    }
    if (!compile())
        return;
    const std::uint64_t id = session_.nextSequence();
    const build::TestReport report = builder_.runTests(executable(), package_.tests, session_.store().buildDir());

    std::string record;
    std::string failed;
    panel_ = ResultPanel{};
    panel_.title = "TESTS  " + std::to_string(report.passed()) + "/" + std::to_string(report.total()) + " passed";
    for (const auto& result : report.cases) {
        record += "== " + result.name + (result.passed ? ": passed" : ": failed") + "\n" + result.process.output + "\n";
        if (result.passed) {
            panel_.lines.emplace_back("✓ " + result.name, theme::ok());
            continue;
        }
        failed += (failed.empty() ? "" : ",") + result.name;
        std::string why = result.process.timedOut ? "time limit exceeded"
            : result.process.exitCode != 0      ? "exit code " + std::to_string(result.process.exitCode)
                                                : "";
        if (why.empty() && result.comparison.firstDifferentLine)
            why = "line " + std::to_string(*result.comparison.firstDifferentLine) + ": expected '"
                + result.comparison.expectedLine + "', got '" + result.comparison.actualLine + "'";
        panel_.lines.emplace_back("✗ " + result.name + ": " + why, theme::bad());
    }
    files::writeAtomically(session_.store().runFile(id), record);

    Event event(EventType::Test);
    event.set("passed", static_cast<long long>(report.passed()))
        .set("total", static_cast<long long>(report.total()))
        .set("failed", failed)
        .set("retained", session_.retainedSummary(editor_.buffer()))
        .set("file", "runs/" + session_.store().runFile(id).filename().string());
    session_.publish(event);
}

void StudentApp::jumpToNextError()
{
    if (errors_.empty()) {
        editor_.setMessage("No compiler messages.");
        return;
    }
    const build::CompilerMessage& message = errors_[nextError_ % errors_.size()];
    ++nextError_;
    editor_.gotoLine(static_cast<std::size_t>(message.line));
    const std::size_t line = editor_.cursorPosition().line;
    const std::size_t column = message.column > 0 ? static_cast<std::size_t>(message.column - 1) : 0;
    editor_.moveCursorTo(editor_.buffer().offsetOf(line, column));
    editor_.setMessage(std::string(build::severityName(message.severity)) + ": " + message.text);
}

void StudentApp::askForNote()
{
    const auto paste = session_.lastPaste();
    if (!paste) {
        editor_.setMessage("There is no paste to add a note to.");
        return;
    }
    const std::uint64_t id = paste->id;
    editor_.startPrompt("Note for paste #" + std::to_string(id) + ": ", false, [this, id](const std::string& text) {
        if (strings::trim(text).empty())
            return;
        Event note(EventType::PasteNote);
        note.set("paste", static_cast<long long>(id)).set("text", text);
        session_.publish(note);
        editor_.setMessage("Note saved for paste #" + std::to_string(id) + ".");
        if (strings::startsWith(notice_, "Paste recorded"))
            notice_.clear();
    });
}

void StudentApp::askToEnd()
{
    if (ended_)
        return;
    editor_.startPrompt("End the exam session? You cannot continue afterwards. (y/n): ", false,
        [this](const std::string& answer) {
            if (!answer.empty() && (answer[0] == 'y' || answer[0] == 'Y'))
                endSession("student");
            else
                editor_.setMessage("The session continues.");
        });
}

void StudentApp::openDashboard()
{
    if (dashboard_ == nullptr) {
        editor_.setMessage("The examiner dashboard is not available in this build.");
        return;
    }
    auto openIt = [this] {
        session_.publish(Event(EventType::DashboardOpened));
        dashboard_->open(terminal_, session_.store().folder());
        session_.publish(Event(EventType::DashboardClosed));
        renderer_.invalidate();
    };
    if (!package_.passcode) {
        openIt();
        return;
    }
    editor_.startPrompt("Examiner passcode: ", true, [this, openIt](const std::string& passcode) {
        if (package_.passcode->matches(passcode)) {
            passcodeFailures_ = 0;
            openIt();
            return;
        }
        Event failed(EventType::PasscodeFailed);
        failed.set("attempts", static_cast<long long>(++passcodeFailures_));
        session_.publish(failed);
        editor_.setMessage("Wrong passcode. This attempt was recorded.");
    });
}

void StudentApp::endSession(const std::string& reason)
{
    if (ended_)
        return;
    save();
    ended_ = true;
    const std::string text = editor_.buffer().text();
    files::writeAtomically(session_.store().submissionDir() / fileName(), text);

    Event end(EventType::SessionEnd);
    end.set("reason", reason)
        .set("submission_hash", crypto::Sha256::hex(text))
        .set("retained", session_.retainedSummary(editor_.buffer()));
    session_.publish(end);
    seal_ = session_.seal();
    editor_.setReadOnly(true);

    Overlay receipt;
    receipt.title = "SESSION ENDED";
    receipt.lines = {
        reason == "time" ? "Time is up. Your file was saved and the session is sealed."
                         : "Your file was saved and the session is sealed.",
        "",
        "Receipt code:   " + seal_->receipt,
        "",
        "Show this code to the invigilator so it can be written on the attendance sheet.",
        "The examiner uses it at the viva to check that the record was not changed.",
    };
    receipt.footer = "Press Enter to close OV_C";
    receipt.endsProgram = true;
    overlay_ = receipt;
}

// ------------------------------------------------------------------ screens

void StudentApp::showOverlay(std::string title, std::vector<std::string> lines)
{
    Overlay overlay;
    overlay.title = std::move(title);
    overlay.lines = std::move(lines);
    overlay_ = overlay;
}

void StudentApp::showRules()
{
    std::vector<std::string> lines;
    for (const auto& line : penalty::describePolicy(package_.policy))
        lines.push_back("• " + line);
    showOverlay("EXAM RULES", lines);
}

void StudentApp::showProblem()
{
    std::vector<std::string> lines{package_.title, ""};
    for (const auto& line : strings::splitLines(package_.problem))
        lines.push_back(line);
    for (std::size_t i = 0; i < package_.tests.size(); ++i) {
        lines.push_back("");
        lines.push_back("Sample " + std::to_string(i + 1) + " input:");
        for (const auto& line : strings::splitLines(package_.tests[i].input))
            lines.push_back("  " + line);
        lines.push_back("Expected output:");
        for (const auto& line : strings::splitLines(package_.tests[i].expectedOutput))
            lines.push_back("  " + line);
    }
    showOverlay("PROBLEM", lines);
}

void StudentApp::showHelp()
{
    showOverlay("HELP", {
        "The editor starts in Insert mode: just type. Esc switches to Normal mode (Vim keys).",
        "",
        "Ctrl+S save            Ctrl+Z undo        Ctrl+Y redo        arrows, Home/End, PgUp/PgDn",
        "F2 problem panel       F5 compile         F6 run             F7 sample tests",
        "F8 next error          Ctrl+N note for the last paste        Ctrl+Q end the exam",
        "",
        "Normal mode: h j k l w b e 0 ^ $ gg G, counts (5j), x X dd yy p P u Ctrl+R,",
        "             d/c/y + motion (d3w, c$), o O i a A I, / search, n N",
        "",
        ":w save   :q end the exam   :compile   :run   :run in.txt   :test   :output",
        ":rules exam rules   :problem full problem   :note <text>   :N go to line N",
    });
}

void StudentApp::setNotice(std::string text)
{
    notice_ = std::move(text);
    noticeSince_ = timeutil::nowSteadyMillis();
}

void StudentApp::checkClock()
{
    if (!notice_.empty() && timeutil::nowSteadyMillis() - noticeSince_ > NoticeLifetime)
        notice_.clear();
    if (ended_)
        return;
    const session::SessionClock& clock = session_.exam().clock();
    if (!clock.hasLimit())
        return;
    const timeutil::Millis remaining = clock.remaining();
    if (remaining <= 0) {
        endSession("time");
        return;
    }
    auto warn = [this](int minutes) {
        Event warning(EventType::TimeWarning);
        warning.set("minutes_left", static_cast<long long>(minutes));
        session_.publish(warning);
        setNotice(std::to_string(minutes) + " minutes left. Save your work (Ctrl+S).");
    };
    if (!warnedTen_ && remaining <= 10 * 60'000 && clock.timeLimit() > 10 * 60'000) {
        warnedTen_ = true;
        warn(10);
    }
    if (!warnedTwo_ && remaining <= 2 * 60'000 && clock.timeLimit() > 2 * 60'000) {
        warnedTwo_ = true;
        warn(2);
    }
}

void StudentApp::draw()
{
    const platform::TerminalSize size = terminal_.size();
    if (frame_.width() != size.columns || frame_.height() != size.rows) {
        frame_.resize(size.columns, size.rows);
        renderer_.invalidate();
    }
    StudentScreenState state;
    state.editor = &editor_;
    state.package = &package_;
    state.fileName = fileName();
    const session::SessionClock& clock = session_.exam().clock();
    if (ended_)
        state.timeText = "session ended";
    else if (clock.hasLimit())
        state.timeText = timeutil::formatDuration(clock.remaining()) + " left";
    else
        state.timeText = "no time limit";
    state.timeShort = clock.hasLimit() && !ended_ && clock.remaining() <= 2 * 60'000;
    state.showProblem = showProblem_;
    state.panel = &panel_;
    state.notice = notice_;
    state.overlay = overlay_ ? &*overlay_ : nullptr;
    screen_.draw(frame_, state);
    terminal_.write(renderer_.render(frame_));
}

std::string StudentApp::fileName() const
{
    return sourceFile_.filename().string();
}

std::filesystem::path StudentApp::executable() const
{
    return build::executablePath(session_.store().buildDir(), sourceFile_.stem().string());
}

} // namespace ovc::app
