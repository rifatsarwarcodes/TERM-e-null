#pragma once

#include "platform/InputEvent.hpp"

#include <memory>
#include <string_view>
#include <vector>

namespace ovc::platform {

struct TerminalSize {
    int columns = 80;
    int rows = 24;
};

// Everything the program needs from a terminal. The real implementations
// talk to the POSIX tty or the Windows console; ScriptedTerminal replays
// prepared input in tests.
class ITerminal {
public:
    virtual ~ITerminal() = default;

    // Raw input, alternate screen, bracketed paste and focus reporting on.
    virtual void start() = 0;
    // Back to the normal screen and the original terminal settings.
    virtual void stop() = 0;

    virtual TerminalSize size() const = 0;

    // Waits up to timeoutMs for input and returns everything that arrived
    // together. Events that arrive in one batch were waiting in the input
    // queue at the same time, which is how bursts of pasted keys are spotted.
    virtual std::vector<InputEvent> read(int timeoutMs) = 0;

    virtual void write(std::string_view data) = 0;

    // True when the window is being closed or the process was asked to stop.
    virtual bool closeRequested() const { return false; }
    // Called once the program has saved its state after closeRequested().
    virtual void shutdownComplete() {}
};

// The terminal for the current platform (POSIX tty or Windows console).
std::unique_ptr<ITerminal> createSystemTerminal();

// Keeps the terminal started for the lifetime of the guard, so it is always
// restored, even when an exception leaves the scope.
class TerminalGuard {
public:
    explicit TerminalGuard(ITerminal& terminal)
        : terminal_(terminal)
    {
        terminal_.start();
    }
    ~TerminalGuard() { terminal_.stop(); }
    TerminalGuard(const TerminalGuard&) = delete;
    TerminalGuard& operator=(const TerminalGuard&) = delete;

private:
    ITerminal& terminal_;
};

} // namespace ovc::platform
