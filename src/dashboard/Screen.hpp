#pragma once

#include "platform/InputEvent.hpp"
#include "ui/ScreenBuffer.hpp"

#include <memory>
#include <string>

namespace ovc::dashboard {

class Screen;

// What a screen wants after handling a key.
struct ScreenResult {
    enum class Action { Stay, Push, Back, Quit };

    Action action = Action::Stay;
    std::unique_ptr<Screen> next;

    static ScreenResult stay() { return {}; }
    static ScreenResult back() { return {Action::Back, nullptr}; }
    static ScreenResult quit() { return {Action::Quit, nullptr}; }
    static ScreenResult push(std::unique_ptr<Screen> screen) { return {Action::Push, std::move(screen)}; }
};

// One full-screen view of the dashboard. Screens are stacked: Enter on an
// item pushes a more detailed screen, Esc goes back.
class Screen {
public:
    virtual ~Screen() = default;
    virtual void draw(ui::ScreenBuffer& screen) = 0;
    virtual ScreenResult handleKey(const platform::KeyEvent& key) = 0;
};

// Shared frame parts so every screen looks the same.
namespace chrome {

void header(ui::ScreenBuffer& screen, const std::string& label, const std::string& text,
    const std::string& chip = "", const ui::Style& chipStyle = {});
void keyBar(ui::ScreenBuffer& screen, const std::string& keys, const std::string& message = "");

} // namespace chrome

} // namespace ovc::dashboard
