#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ovc::strings {

std::string trim(std::string_view text);
std::string trimRight(std::string_view text);
bool startsWith(std::string_view text, std::string_view prefix);
bool endsWith(std::string_view text, std::string_view suffix);
std::string toLower(std::string_view text);
std::string toUpper(std::string_view text);

// Splits on every occurrence of the separator. "a,,b" -> {"a", "", "b"}.
std::vector<std::string> split(std::string_view text, char separator);

// Splits into lines, accepting "\n" and "\r\n". A trailing newline does not
// produce an extra empty line.
std::vector<std::string> splitLines(std::string_view text);

std::string join(const std::vector<std::string>& parts, std::string_view separator);
std::string replaceAll(std::string text, std::string_view from, std::string_view to);

std::size_t countLines(std::string_view text);
bool isBlank(std::string_view text);

// Parses a whole string as a non-negative integer. Returns false on any
// stray character or overflow.
bool parseUnsigned(std::string_view text, std::uint64_t& value);
bool parseInt(std::string_view text, long long& value);
bool parseDouble(std::string_view text, double& value);

std::string toHex(const std::uint8_t* bytes, std::size_t length);

// Number of terminal columns a UTF-8 string occupies, counting one column
// per code point (wide characters are not used by this program).
std::size_t displayWidth(std::string_view utf8);

// Removes the last UTF-8 code point. Used for backspace in prompts.
void popCodePoint(std::string& utf8);

} // namespace ovc::strings
