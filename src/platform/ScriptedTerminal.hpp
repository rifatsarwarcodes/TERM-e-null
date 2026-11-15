#pragma once

#include "platform/ITerminal.hpp"

#include <deque>
#include <string>
#include <vector>

namespace ovc::platform {

// A terminal that replays prepared input batches and records all output.
// Used by tests to drive the editor and the dashboard without a real tty.
class ScriptedTerminal final : public ITerminal {
public:
    explicit ScriptedTerminal(TerminalSize size = {100, 30});

    // Queues one batch of events, as if they arrived together.
    void queue(std::vector<InputEvent> batch);
    // Queues each character of the text as a separate typed key.
    void type(const std::string& text);
    void press(KeyEvent key);
    void requestClose() { closeRequested_ = true; }
    void resize(TerminalSize size) { size_ = size; }

    bool hasInput() const { return !batches_.empty(); }
    const std::string& output() const { return output_; }
    void clearOutput() { output_.clear(); }

    void start() override { started_ = true; }
    void stop() override { started_ = false; }
    bool started() const { return started_; }
    TerminalSize size() const override { return size_; }
    std::vector<InputEvent> read(int timeoutMs) override;
    void write(std::string_view data) override { output_.append(data.data(), data.size()); }
    bool closeRequested() const override { return closeRequested_; }

private:
    TerminalSize size_;
    std::deque<std::vector<InputEvent>> batches_;
    std::string output_;
    bool started_ = false;
    bool closeRequested_ = false;
};

} // namespace ovc::platform
