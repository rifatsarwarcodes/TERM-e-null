#pragma once

#include <iosfwd>
#include <string>

namespace ovc::platform {

// Turns echo of typed characters on or off for line-based input, used for
// reading the examiner passcode outside the full-screen interface.
void setInputEcho(bool enabled);

// Reads one line without showing what is typed.
std::string readSecretLine(std::istream& in, std::ostream& out);

} // namespace ovc::platform
