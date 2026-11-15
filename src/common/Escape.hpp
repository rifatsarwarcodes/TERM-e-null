#pragma once

#include <string>
#include <string_view>

// Escaping for the one-line text formats (event log, journal, session.info).
// The characters | ; = \ and line breaks are written as backslash sequences,
// so a field can hold any text and still be split safely.
namespace ovc::escape {

std::string encode(std::string_view text);
std::string decode(std::string_view text);

// Splits on `separator` while ignoring escaped separators.
std::size_t findUnescaped(std::string_view text, char separator, std::size_t from = 0);

} // namespace ovc::escape
