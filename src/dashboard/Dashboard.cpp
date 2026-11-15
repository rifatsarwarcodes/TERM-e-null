#include "dashboard/Dashboard.hpp"

#include "common/Strings.hpp"
#include "ui/Theme.hpp"

#include <variant>

namespace ovc::dashboard {

namespace theme = ui::theme;

void chrome::header(ui::ScreenBuffer& screen, const std::string& label, const std::string& text,
    const std::string& chip, const ui::Style& chipStyle)
{
    screen.fill({0, 0, screen.width(), 1}, theme::topBar());
    const int x = screen.text(0, 0, " " + label + " ", theme::brand());
    screen.text(x + 1, 0, text, theme::topBar(), screen.width() - x - 2);
    if (!chip.empty()) {
        const std::string padded = " " + chip + " ";
        screen.text(screen.width() - static_cast<int>(strings::displayWidth(padded)), 0, padded, chipStyle);
    }
}

void chrome::keyBar(ui::ScreenBuffer& screen, const std::string& keys, const std::string& message)
{
    const int y = screen.height() - 1;
    screen.fill({0, y, screen.width(), 1}, theme::keyBar());
    if (!message.empty())
        screen.text(1, y, message, theme::keyBar().withForeground({221}), screen.width() - 2);
    else
        screen.text(1, y, keys, theme::keyBar(), screen.width() - 2);
}

Dashboard::Dashboard(std::unique_ptr<Screen> root)
{
    stack_.push_back(std::move(root));
}

void Dashboard::run(platform::ITerminal& terminal)
{
    renderer_.invalidate();
    while (step(terminal, 250)) {
    }
}

bool Dashboard::step(platform::ITerminal& terminal, int timeoutMs)
{
    if (stack_.empty() || terminal.closeRequested())
        return false;
    const platform::TerminalSize size = terminal.size();
    if (frame_.width() != size.columns || frame_.height() != size.rows) {
        frame_.resize(size.columns, size.rows);
        renderer_.invalidate();
    }
    frame_.clear();
    stack_.back()->draw(frame_);
    terminal.write(renderer_.render(frame_));

    for (const auto& event : terminal.read(timeoutMs)) {
        if (const auto* key = std::get_if<platform::KeyEvent>(&event))
            handle(*key);
        else if (std::holds_alternative<platform::ResizeEvent>(event))
            renderer_.invalidate();
        if (stack_.empty())
            break;
    }
    return !stack_.empty();
}

void Dashboard::handle(const platform::KeyEvent& key)
{
    ScreenResult result = stack_.back()->handleKey(key);
    switch (result.action) {
    case ScreenResult::Action::Stay: break;
    case ScreenResult::Action::Push: stack_.push_back(std::move(result.next)); break;
    case ScreenResult::Action::Back: stack_.pop_back(); break;
    case ScreenResult::Action::Quit: stack_.clear(); break;
    }
}

} // namespace ovc::dashboard
