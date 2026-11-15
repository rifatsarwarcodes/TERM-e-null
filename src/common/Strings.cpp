#include "common/Strings.hpp"

#include <cctype>
#include <cstdlib>
#include <limits>

namespace ovc::strings {

namespace {

bool isSpace(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v';
}

} // namespace

std::string trim(std::string_view text)
{
    std::size_t begin = 0;
    std::size_t end = text.size();
    while (begin < end && isSpace(text[begin]))
        ++begin;
    while (end > begin && isSpace(text[end - 1]))
        --end;
    return std::string(text.substr(begin, end - begin));
}

std::string trimRight(std::string_view text)
{
    std::size_t end = text.size();
    while (end > 0 && isSpace(text[end - 1]))
        --end;
    return std::string(text.substr(0, end));
}

bool startsWith(std::string_view text, std::string_view prefix)
{
    return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
}

bool endsWith(std::string_view text, std::string_view suffix)
{
    return text.size() >= suffix.size()
        && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string toLower(std::string_view text)
{
    std::string result(text);
    for (char& c : result)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return result;
}

std::string toUpper(std::string_view text)
{
    std::string result(text);
    for (char& c : result)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return result;
}

std::vector<std::string> split(std::string_view text, char separator)
{
    std::vector<std::string> parts;
    std::size_t start = 0;
    for (std::size_t i = 0; i <= text.size(); ++i) {
        if (i == text.size() || text[i] == separator) {
            parts.emplace_back(text.substr(start, i - start));
            start = i + 1;
        }
    }
    return parts;
}

std::vector<std::string> splitLines(std::string_view text)
{
    std::vector<std::string> lines;
    std::size_t start = 0;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\n') {
            std::size_t end = i;
            if (end > start && text[end - 1] == '\r')
                --end;
            lines.emplace_back(text.substr(start, end - start));
            start = i + 1;
        }
    }
    if (start < text.size())
        lines.emplace_back(text.substr(start));
    return lines;
}

std::string join(const std::vector<std::string>& parts, std::string_view separator)
{
    std::string result;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i > 0)
            result += separator;
        result += parts[i];
    }
    return result;
}

std::string replaceAll(std::string text, std::string_view from, std::string_view to)
{
    if (from.empty())
        return text;
    std::size_t pos = 0;
    while ((pos = text.find(from, pos)) != std::string::npos) {
        text.replace(pos, from.size(), to);
        pos += to.size();
    }
    return text;
}

std::size_t countLines(std::string_view text)
{
    if (text.empty())
        return 0;
    std::size_t lines = 1;
    for (std::size_t i = 0; i + 1 < text.size(); ++i)
        if (text[i] == '\n')
            ++lines;
    return lines;
}

bool isBlank(std::string_view text)
{
    for (char c : text)
        if (!isSpace(c))
            return false;
    return true;
}

bool parseUnsigned(std::string_view text, std::uint64_t& value)
{
    if (text.empty())
        return false;
    std::uint64_t result = 0;
    for (char c : text) {
        if (c < '0' || c > '9')
            return false;
        const auto digit = static_cast<std::uint64_t>(c - '0');
        if (result > (std::numeric_limits<std::uint64_t>::max() - digit) / 10)
            return false;
        result = result * 10 + digit;
    }
    value = result;
    return true;
}

bool parseInt(std::string_view text, long long& value)
{
    bool negative = false;
    if (!text.empty() && (text[0] == '-' || text[0] == '+')) {
        negative = text[0] == '-';
        text.remove_prefix(1);
    }
    std::uint64_t magnitude = 0;
    if (!parseUnsigned(text, magnitude)
        || magnitude > static_cast<std::uint64_t>(std::numeric_limits<long long>::max()))
        return false;
    value = negative ? -static_cast<long long>(magnitude) : static_cast<long long>(magnitude);
    return true;
}

bool parseDouble(std::string_view text, double& value)
{
    const std::string copy(text);
    if (copy.empty())
        return false;
    char* end = nullptr;
    const double result = std::strtod(copy.c_str(), &end);
    if (end != copy.c_str() + copy.size())
        return false;
    value = result;
    return true;
}

std::string toHex(const std::uint8_t* bytes, std::size_t length)
{
    static const char digits[] = "0123456789abcdef";
    std::string hex;
    hex.reserve(length * 2);
    for (std::size_t i = 0; i < length; ++i) {
        hex.push_back(digits[bytes[i] >> 4]);
        hex.push_back(digits[bytes[i] & 0x0F]);
    }
    return hex;
}

std::size_t displayWidth(std::string_view utf8)
{
    std::size_t columns = 0;
    for (char c : utf8)
        if ((static_cast<unsigned char>(c) & 0xC0) != 0x80)
            ++columns;
    return columns;
}

void popCodePoint(std::string& utf8)
{
    while (!utf8.empty()) {
        const auto last = static_cast<unsigned char>(utf8.back());
        utf8.pop_back();
        if ((last & 0xC0) != 0x80)
            break;
    }
}

} // namespace ovc::strings
