// Terminal for the Windows console (Windows 10 or later).
//
// Output uses virtual-terminal (ANSI) sequences. Input is read as console
// input records: with ENABLE_VIRTUAL_TERMINAL_INPUT the console delivers the
// same escape sequences as a Unix terminal, so the shared InputDecoder handles
// keys, bracketed paste and focus reports. FOCUS_EVENT records are also used.
#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "platform/ITerminal.hpp"
#include "platform/InputDecoder.hpp"
#include "platform/TerminalControl.hpp"

#include <atomic>
#include <string>

#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#ifndef DISABLE_NEWLINE_AUTO_RETURN
#define DISABLE_NEWLINE_AUTO_RETURN 0x0008
#endif
#ifndef ENABLE_VIRTUAL_TERMINAL_INPUT
#define ENABLE_VIRTUAL_TERMINAL_INPUT 0x0200
#endif

namespace ovc::platform {

namespace {

std::atomic<bool> closeSignalled{false};
std::atomic<bool> shutdownDone{false};

BOOL WINAPI onConsoleControl(DWORD type)
{
    if (type == CTRL_CLOSE_EVENT || type == CTRL_LOGOFF_EVENT || type == CTRL_SHUTDOWN_EVENT
        || type == CTRL_BREAK_EVENT) {
        closeSignalled = true;
        // Windows ends the process when this handler returns, so give the main
        // loop a few seconds to save the file and seal the log.
        for (int i = 0; i < 40 && !shutdownDone; ++i)
            Sleep(100);
        return TRUE;
    }
    return FALSE;
}

constexpr DWORD EscapeWaitMs = 25;

void appendUtf8(std::string& out, unsigned int codePoint)
{
    if (codePoint < 0x80) {
        out.push_back(static_cast<char>(codePoint));
    } else if (codePoint < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
        out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    } else if (codePoint < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
        out.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
        out.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    }
}

// Used only when the console does not support virtual-terminal input.
bool translateVirtualKey(WORD key, KeyEvent& event)
{
    switch (key) {
    case VK_UP: event = KeyEvent::special(KeyCode::Up); return true;
    case VK_DOWN: event = KeyEvent::special(KeyCode::Down); return true;
    case VK_LEFT: event = KeyEvent::special(KeyCode::Left); return true;
    case VK_RIGHT: event = KeyEvent::special(KeyCode::Right); return true;
    case VK_HOME: event = KeyEvent::special(KeyCode::Home); return true;
    case VK_END: event = KeyEvent::special(KeyCode::End); return true;
    case VK_PRIOR: event = KeyEvent::special(KeyCode::PageUp); return true;
    case VK_NEXT: event = KeyEvent::special(KeyCode::PageDown); return true;
    case VK_DELETE: event = KeyEvent::special(KeyCode::Delete); return true;
    case VK_INSERT: event = KeyEvent::special(KeyCode::Insert); return true;
    case VK_F1: event = KeyEvent::special(KeyCode::F1); return true;
    case VK_F2: event = KeyEvent::special(KeyCode::F2); return true;
    case VK_F3: event = KeyEvent::special(KeyCode::F3); return true;
    case VK_F4: event = KeyEvent::special(KeyCode::F4); return true;
    case VK_F5: event = KeyEvent::special(KeyCode::F5); return true;
    case VK_F6: event = KeyEvent::special(KeyCode::F6); return true;
    case VK_F7: event = KeyEvent::special(KeyCode::F7); return true;
    case VK_F8: event = KeyEvent::special(KeyCode::F8); return true;
    case VK_F9: event = KeyEvent::special(KeyCode::F9); return true;
    case VK_F10: event = KeyEvent::special(KeyCode::F10); return true;
    case VK_F11: event = KeyEvent::special(KeyCode::F11); return true;
    case VK_F12: event = KeyEvent::special(KeyCode::F12); return true;
    default: return false;
    }
}

class WinConsoleTerminal final : public ITerminal {
public:
    ~WinConsoleTerminal() override { stop(); }

    void start() override
    {
        if (started_)
            return;
        input_ = GetStdHandle(STD_INPUT_HANDLE);
        output_ = GetStdHandle(STD_OUTPUT_HANDLE);
        GetConsoleMode(input_, &originalInputMode_);
        GetConsoleMode(output_, &originalOutputMode_);
        originalOutputCodePage_ = GetConsoleOutputCP();
        originalInputCodePage_ = GetConsoleCP();
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);

        // No line editing, no echo, Ctrl+C arrives as a key. Quick-edit is
        // kept as it was so mouse selection and right-click paste still work.
        DWORD inputMode = ENABLE_WINDOW_INPUT | ENABLE_EXTENDED_FLAGS
            | (originalInputMode_ & ENABLE_QUICK_EDIT_MODE);
        virtualInput_ = SetConsoleMode(input_, inputMode | ENABLE_VIRTUAL_TERMINAL_INPUT) != 0;
        if (!virtualInput_)
            SetConsoleMode(input_, inputMode);

        SetConsoleMode(output_,
            originalOutputMode_ | ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN);
        SetConsoleCtrlHandler(onConsoleControl, TRUE);

        write(control::EnterFullScreen);
        started_ = true;
    }

    void stop() override
    {
        if (!started_)
            return;
        write(control::LeaveFullScreen);
        SetConsoleMode(input_, originalInputMode_);
        SetConsoleMode(output_, originalOutputMode_);
        SetConsoleOutputCP(originalOutputCodePage_);
        SetConsoleCP(originalInputCodePage_);
        SetConsoleCtrlHandler(onConsoleControl, FALSE);
        started_ = false;
    }

    TerminalSize size() const override
    {
        CONSOLE_SCREEN_BUFFER_INFO info;
        if (GetConsoleScreenBufferInfo(output_, &info)) {
            return {info.srWindow.Right - info.srWindow.Left + 1, info.srWindow.Bottom - info.srWindow.Top + 1};
        }
        return {};
    }

    std::vector<InputEvent> read(int timeoutMs) override
    {
        std::vector<InputEvent> events;
        if (WaitForSingleObject(input_, static_cast<DWORD>(timeoutMs)) == WAIT_OBJECT_0)
            readAvailable(events);

        if (decoder_.hasPending()) {
            if (WaitForSingleObject(input_, EscapeWaitMs) == WAIT_OBJECT_0)
                readAvailable(events);
            else
                decoder_.flushPending(events);
        }
        return events;
    }

    void write(std::string_view data) override
    {
        while (!data.empty()) {
            DWORD written = 0;
            if (!WriteFile(output_, data.data(), static_cast<DWORD>(data.size()), &written, nullptr) || written == 0)
                return;
            data.remove_prefix(written);
        }
    }

    bool closeRequested() const override { return closeSignalled; }
    void shutdownComplete() override { shutdownDone = true; }

private:
    // Reads every input record waiting in the console queue as one batch.
    void readAvailable(std::vector<InputEvent>& events)
    {
        std::string text;
        auto flushText = [&] {
            if (!text.empty()) {
                decoder_.feed(text, events);
                text.clear();
            }
        };

        DWORD available = 0;
        while (GetNumberOfConsoleInputEvents(input_, &available) && available > 0) {
            INPUT_RECORD records[256];
            DWORD count = 0;
            if (!ReadConsoleInputW(input_, records, 256, &count) || count == 0)
                break;
            for (DWORD i = 0; i < count; ++i)
                handleRecord(records[i], text, events, flushText);
        }
        flushText();
    }

    template <typename Flush>
    void handleRecord(const INPUT_RECORD& record, std::string& text, std::vector<InputEvent>& events, Flush& flushText)
    {
        switch (record.EventType) {
        case KEY_EVENT: {
            const KEY_EVENT_RECORD& key = record.Event.KeyEvent;
            if (!key.bKeyDown)
                return;
            const wchar_t unit = key.uChar.UnicodeChar;
            for (WORD repeat = 0; repeat < key.wRepeatCount; ++repeat) {
                if (unit != 0) {
                    appendUtf16Unit(text, unit);
                } else if (!virtualInput_) {
                    KeyEvent translated;
                    if (translateVirtualKey(key.wVirtualKeyCode, translated)) {
                        flushText();
                        events.emplace_back(translated);
                    }
                }
            }
            return;
        }
        case FOCUS_EVENT:
            flushText();
            events.emplace_back(FocusEvent{record.Event.FocusEvent.bSetFocus != 0});
            return;
        case WINDOW_BUFFER_SIZE_EVENT:
            flushText();
            events.emplace_back(ResizeEvent{});
            return;
        default:
            return;
        }
    }

    void appendUtf16Unit(std::string& text, wchar_t unit)
    {
        const auto value = static_cast<unsigned int>(unit);
        if (value >= 0xD800 && value <= 0xDBFF) {
            highSurrogate_ = value;
            return;
        }
        if (value >= 0xDC00 && value <= 0xDFFF) {
            if (highSurrogate_ != 0)
                appendUtf8(text, 0x10000 + ((highSurrogate_ - 0xD800) << 10) + (value - 0xDC00));
            highSurrogate_ = 0;
            return;
        }
        appendUtf8(text, value);
    }

    HANDLE input_ = INVALID_HANDLE_VALUE;
    HANDLE output_ = INVALID_HANDLE_VALUE;
    DWORD originalInputMode_ = 0;
    DWORD originalOutputMode_ = 0;
    UINT originalOutputCodePage_ = 0;
    UINT originalInputCodePage_ = 0;
    bool virtualInput_ = false;
    bool started_ = false;
    unsigned int highSurrogate_ = 0;
    InputDecoder decoder_;
};

} // namespace

std::unique_ptr<ITerminal> createSystemTerminal()
{
    return std::make_unique<WinConsoleTerminal>();
}

} // namespace ovc::platform

#endif // _WIN32
