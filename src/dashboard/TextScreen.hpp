#pragma once

#include "dashboard/Screen.hpp"

#include <string>
#include <vector>

namespace ovc::dashboard {

// A scrollable text viewer: pasted text, program output, reports.
class TextScreen final : public Screen {
public:
    TextScreen(std::string title, std::vector<std::string> lines);

    void draw(ui::ScreenBuffer& screen) override;
    ScreenResult handleKey(const platform::KeyEvent& key) override;

private:
    std::string title_;
    std::vector<std::string> lines_;
    std::size_t top_ = 0;
    int rows_ = 20;
};

} // namespace ovc::dashboard
