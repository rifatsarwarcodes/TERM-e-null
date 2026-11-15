#pragma once

#include "ui/Style.hpp"

// The colours used across the editor and the dashboard, in one place.
namespace ovc::ui::theme {

inline Style text() { return {}; }
inline Style dim() { return {{245}, {}}; }
inline Style heading() { return Style{{111}, {}}.withBold(); }
inline Style border() { return {{60}, {}}; }
inline Style title() { return Style{{111}, {}}.withBold(); }

inline Style topBar() { return {{255}, {24}}; }
inline Style brand() { return Style{{16}, {220}}.withBold(); }
inline Style subBar() { return {{250}, {236}}; }
inline Style keyBar() { return {{252}, {237}}; }
inline Style notice() { return {{222}, {58}}; }
inline Style commandLine() { return {}; }

inline Style modeInsert() { return Style{{16}, {78}}.withBold(); }
inline Style modeNormal() { return Style{{16}, {75}}.withBold(); }
inline Style modeCommand() { return Style{{16}, {141}}.withBold(); }
inline Style modeLocked() { return Style{{255}, {160}}.withBold(); }

inline Style lineNumber() { return {{242}, {}}; }
inline Style currentLineNumber() { return Style{{221}, {}}.withBold(); }
inline Color currentLineBackground() { return {235}; }
inline Style searchMatch() { return {{16}, {136}}; }
inline Style selection() { return {{255}, {25}}; }

inline Style keyword() { return {{176}, {}}; }
inline Style stringLiteral() { return {{114}, {}}; }
inline Style comment() { return {{244}, {}}; }
inline Style number() { return {{215}, {}}; }
inline Style preprocessor() { return {{140}, {}}; }

inline Style ok() { return {{78}, {}}; }
inline Style warn() { return {{221}, {}}; }
inline Style bad() { return {{203}, {}}; }
inline Style okChip() { return Style{{16}, {78}}.withBold(); }
inline Style warnChip() { return Style{{16}, {221}}.withBold(); }
inline Style badChip() { return Style{{16}, {203}}.withBold(); }

inline Style originTyped() { return {}; }
inline Style originLoaded() { return {{250}, {}}; }
inline Style originInside() { return {{117}, {}}; }
inline Style originSelf() { return {{111}, {}}; }
inline Style originAllowed() { return {{114}, {}}; }
inline Style originOutside() { return {{210}, {}}; }

} // namespace ovc::ui::theme
