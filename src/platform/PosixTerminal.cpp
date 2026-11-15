// Terminal for Linux and macOS: termios raw mode and poll() for input.
#ifndef _WIN32

#include "platform/ITerminal.hpp"
#include "platform/InputDecoder.hpp"
#include "platform/TerminalControl.hpp"

#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <poll.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

namespace ovc::platform {

namespace {

std::atomic<bool> resizePending{false};
std::atomic<bool> stopRequested{false};

extern "C" void onResizeSignal(int) { resizePending = true; }
extern "C" void onStopSignal(int) { stopRequested = true; }

// How long to wait after a lone ESC before deciding it is the Escape key and
// not the start of an escape sequence.
constexpr int EscapeWaitMs = 25;

class PosixTerminal final : public ITerminal {
public:
    ~PosixTerminal() override { stop(); }

    void start() override
    {
        if (started_)
            return;
        if (tcgetattr(STDIN_FILENO, &original_) == 0) {
            termios raw = original_;
            raw.c_iflag &= static_cast<tcflag_t>(~(BRKINT | ICRNL | INPCK | ISTRIP | IXON));
            raw.c_oflag &= static_cast<tcflag_t>(~OPOST);
            raw.c_cflag |= CS8;
            raw.c_lflag &= static_cast<tcflag_t>(~(ECHO | ICANON | IEXTEN | ISIG));
            raw.c_cc[VMIN] = 0;
            raw.c_cc[VTIME] = 0;
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
            haveOriginal_ = true;
        }
        std::signal(SIGWINCH, onResizeSignal);
        std::signal(SIGTERM, onStopSignal);
        std::signal(SIGHUP, onStopSignal);
        write(control::EnterFullScreen);
        started_ = true;
    }

    void stop() override
    {
        if (!started_)
            return;
        write(control::LeaveFullScreen);
        if (haveOriginal_)
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_);
        std::signal(SIGWINCH, SIG_DFL);
        started_ = false;
    }

    TerminalSize size() const override
    {
        winsize window{};
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &window) == 0 && window.ws_col > 0 && window.ws_row > 0)
            return {window.ws_col, window.ws_row};
        return {};
    }

    std::vector<InputEvent> read(int timeoutMs) override
    {
        std::vector<InputEvent> events;
        if (resizePending.exchange(false))
            events.emplace_back(ResizeEvent{});

        if (waitForInput(timeoutMs))
            readAvailable(events);

        // A lone ESC at the end: wait briefly for the rest of a sequence.
        if (decoder_.hasPending()) {
            if (waitForInput(EscapeWaitMs))
                readAvailable(events);
            else
                decoder_.flushPending(events);
        }
        return events;
    }

    void write(std::string_view data) override
    {
        while (!data.empty()) {
            const ssize_t written = ::write(STDOUT_FILENO, data.data(), data.size());
            if (written < 0) {
                if (errno == EINTR)
                    continue;
                return;
            }
            data.remove_prefix(static_cast<std::size_t>(written));
        }
    }

    bool closeRequested() const override { return stopRequested; }

private:
    static bool waitForInput(int timeoutMs)
    {
        pollfd descriptor{STDIN_FILENO, POLLIN, 0};
        const int ready = poll(&descriptor, 1, timeoutMs);
        return ready > 0 && (descriptor.revents & POLLIN) != 0;
    }

    // Reads everything that is waiting right now. All of it counts as one
    // batch, which the paste detection uses as the input-queue depth.
    void readAvailable(std::vector<InputEvent>& events)
    {
        char chunk[4096];
        do {
            const ssize_t count = ::read(STDIN_FILENO, chunk, sizeof chunk);
            if (count <= 0)
                break;
            decoder_.feed(std::string_view(chunk, static_cast<std::size_t>(count)), events);
        } while (waitForInput(0) || decoder_.inPaste());
    }

    termios original_{};
    bool haveOriginal_ = false;
    bool started_ = false;
    InputDecoder decoder_;
};

} // namespace

std::unique_ptr<ITerminal> createSystemTerminal()
{
    return std::make_unique<PosixTerminal>();
}

} // namespace ovc::platform

#endif // _WIN32
