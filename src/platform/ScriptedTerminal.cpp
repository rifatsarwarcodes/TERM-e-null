#include "platform/ScriptedTerminal.hpp"

namespace ovc::platform {

ScriptedTerminal::ScriptedTerminal(TerminalSize size)
    : size_(size)
{
}

void ScriptedTerminal::queue(std::vector<InputEvent> batch)
{
    batches_.push_back(std::move(batch));
}

void ScriptedTerminal::type(const std::string& text)
{
    for (char c : text) {
        if (c == '\n')
            press(KeyEvent::special(KeyCode::Enter));
        else
            press(KeyEvent::character(std::string(1, c)));
    }
}

void ScriptedTerminal::press(KeyEvent key)
{
    queue({InputEvent{std::move(key)}});
}

std::vector<InputEvent> ScriptedTerminal::read(int)
{
    if (batches_.empty())
        return {};
    auto batch = std::move(batches_.front());
    batches_.pop_front();
    return batch;
}

} // namespace ovc::platform
