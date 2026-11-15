#pragma once

#include "session/Event.hpp"
#include "ui/Style.hpp"

#include <string>

namespace ovc::dashboard {

// One-line description of an event for the timeline, e.g.
// "outside | 1240 ch | 46 ln" for a paste.
std::string summariseEvent(const session::Event& event);

// Colour used for the event's type in the timeline.
ui::Style eventStyle(const session::Event& event);

} // namespace ovc::dashboard
