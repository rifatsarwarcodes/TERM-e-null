#pragma once

#include "dashboard/Screen.hpp"
#include "platform/ITerminal.hpp"
#include "ui/AnsiRenderer.hpp"

#include <memory>
#include <vector>

namespace ovc::dashboard {

// Runs a stack of screens on a terminal that is already started. Leaving the
// first screen (Esc) or pressing Q ends the dashboard.
class Dashboard {
public:
    explicit Dashboard(std::unique_ptr<Screen> root);

    void run(platform::ITerminal& terminal);
    // One iteration: draw, then handle one batch of input. Returns false when done.
    bool step(platform::ITerminal& terminal, int timeoutMs);

    bool done() const { return stack_.empty(); }

private:
    void handle(const platform::KeyEvent& key);

    std::vector<std::unique_ptr<Screen>> stack_;
    ui::ScreenBuffer frame_;
    ui::AnsiRenderer renderer_;
};

} // namespace ovc::dashboard
