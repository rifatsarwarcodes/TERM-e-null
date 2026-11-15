#include "common/Escape.hpp"

namespace ovc::escape {

std::string encode(std::string_view text)
{
    std::string out;
    out.reserve(text.size());
    for (char c : text) {
        switch (c) {
        case '\\': out += "\\\\"; break;
        case '|': out += "\\p"; break;
        case ';': out += "\\s"; break;
        case '=': out += "\\e"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        default: out.push_back(c); break;
        }
    }
    return out;
}

std::string decode(std::string_view text)
{
    std::string out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] != '\\' || i + 1 >= text.size()) {
            out.push_back(text[i]);
            continue;
        }
        switch (text[++i]) {
        case 'p': out.push_back('|'); break;
        case 's': out.push_back(';'); break;
        case 'e': out.push_back('='); break;
        case 'n': out.push_back('\n'); break;
        case 'r': out.push_back('\r'); break;
        default: out.push_back(text[i]); break;
        }
    }
    return out;
}

std::size_t findUnescaped(std::string_view text, char separator, std::size_t from)
{
    for (std::size_t i = from; i < text.size(); ++i) {
        if (text[i] == '\\') {
            ++i;
            continue;
        }
        if (text[i] == separator)
            return i;
    }
    return std::string_view::npos;
}

} // namespace ovc::escape
