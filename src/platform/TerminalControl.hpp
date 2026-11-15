#pragma once

// Escape sequences sent when the full-screen interface starts and stops.
// Shared by the POSIX and Windows terminals.
namespace ovc::platform::control {

inline constexpr const char* EnterFullScreen =
    "\x1b[?1049h"  // alternate screen
    "\x1b[?2004h"  // bracketed paste: pasted text arrives between markers
    "\x1b[?1004h"  // focus reporting: ESC [ I / ESC [ O
    "\x1b[H\x1b[2J";

inline constexpr const char* LeaveFullScreen =
    "\x1b[0m"
    "\x1b[?1004l"
    "\x1b[?2004l"
    "\x1b[?25h"    // show the cursor again
    "\x1b[?1049l";

} // namespace ovc::platform::control
