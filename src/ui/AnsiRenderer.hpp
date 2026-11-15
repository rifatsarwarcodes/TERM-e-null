#pragma once

#include "ui/ScreenBuffer.hpp"

#include <string>

namespace ovc::ui {

// Turns a frame into ANSI escape sequences. Only rows that differ from the
// previous frame are written, and the whole update is returned as a single
// string so it reaches the terminal in one write (no flicker).
class AnsiRenderer {
public:
    std::string render(const ScreenBuffer& frame);

    // Forces a full redraw next time (after a resize or when the terminal
    // was used by another program, e.g. a :run).
    void invalidate() { valid_ = false; }

private:
    bool rowChanged(const ScreenBuffer& frame, int y) const;
    static void appendStyle(std::string& out, const Style& style);

    ScreenBuffer previous_;
    bool valid_ = false;
};

} // namespace ovc::ui
